#include "ContextUSB.h"
#include <iostream>
#include <iomanip>

namespace USB
{

  ContextUSB::ContextUSB()
  {
    init();
  }

  ContextUSB::~ContextUSB()
  {
    for (auto transfer : transfers)
    {
      libusb_cancel_transfer(transfer);
      libusb_free_transfer(transfer);
    }

    if (ctx)
    {
      libusb_exit(ctx);
    }
  }

  void LIBUSB_CALL ContextUSB::mouse_callback(libusb_transfer* transfer)
  {
    TransferData* tdata = reinterpret_cast<TransferData*>(transfer->user_data);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
    {
      unsigned char* data = transfer->buffer;
      std::cout << "Mouse event: ";

      for (int i = 0; i < transfer->actual_length; i++)
      {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
      }
      std::cout << std::endl;

      if (tdata && tdata->callback)
      {
        tdata->callback(data, transfer->actual_length);
      }

      int res = libusb_submit_transfer(transfer);
      if (res != 0)
      {
        std::cerr << "Failed to resubmit transfer: " << libusb_error_name(res) << std::endl;
      }
    }
    else
    {
      std::cerr << "Transfer failed: " << libusb_error_name(transfer->status) << std::endl;
    }
  }

  void ContextUSB::init()
  {
    if (libusb_init(&ctx) < 0)
    {
      std::cerr << "Failed to initialize libusb" << std::endl;
      return;
    }

    getMice();

    constexpr unsigned int maxLength = 8;

    for (auto& mouse : mice)
    {
      std::cout << "------------" << std::endl;
      TransferData tdata;
      tdata.callback = [](unsigned char* data, int length)
      {
        std::cout << "Mouse event: ";
        for (int i = 0; i < length; i++)
        {
          std::cout << std::hex << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::endl;
      };
      int result = listenToDevice(mouse, maxLength, &tdata);
      if (result != EXIT_SUCCESS)
      {
        std::cerr << "Failed to listen to device" << std::endl;
      }
      break; // just do first mouse in the vector for now
    }

    struct timeval timeout = { 0, 1000 };  // 1 millisecond timeout

    while (true)
    {
      int err = libusb_handle_events_timeout_completed(ctx, &timeout, nullptr);
      if (err != 0)
      {
        std::cerr << "Error handling events: " << libusb_error_name(err) << std::endl;
        break;
      }
    }
  }

  void ContextUSB::getMice()
  {
    libusb_device** devs;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0)
    {
      std::cerr << "Failed to get device list: " << libusb_error_name(cnt) << std::endl;
      return;
    }

    for (ssize_t i = 0; i < cnt; i++)
    {
      libusb_device* dev = devs[i];
      libusb_device_descriptor desc;
      if (libusb_get_device_descriptor(dev, &desc) < 0)
      {
        continue;
      }

      if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE)
      {
        libusb_config_descriptor* config;
        if (libusb_get_config_descriptor(dev, 0, &config) == 0)
        {
          for (int k = 0; k < config->bNumInterfaces; k++)
          {
            const libusb_interface* inter = &config->interface[k];
            for (int l = 0; l < inter->num_altsetting; l++)
            {
              const libusb_interface_descriptor* interdesc = &inter->altsetting[l];
              if (interdesc->bInterfaceClass == LIBUSB_CLASS_HID &&
                interdesc->bInterfaceSubClass == 1 &&
                interdesc->bInterfaceProtocol == 2)
              {
                for (int m = 0; m < interdesc->bNumEndpoints; m++)
                {
                  const libusb_endpoint_descriptor* epdesc = &interdesc->endpoint[m];
                  if ((epdesc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT &&
                    (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
                  {
                    mice.push_back(Mouse(desc.idVendor, desc.idProduct, epdesc->bEndpointAddress));
                  }
                }
              }
            }
          }
          libusb_free_config_descriptor(config);
        }
      }
    }
    libusb_free_device_list(devs, 1);
  }

  int ContextUSB::listenToDevice(Mouse& mouse, unsigned int max_length, TransferData* tdata)
  {
    libusb_device_handle* handle = libusb_open_device_with_vid_pid(ctx, mouse.vid, mouse.pid);
    if (!handle)
    {
      std::cerr << "Error opening the device" << std::endl;
      return EXIT_FAILURE;
    }

    std::cout << "Opened device with VID: " << std::hex << mouse.vid << " PID: " << mouse.pid << std::endl;

    libusb_set_auto_detach_kernel_driver(handle, true);

    libusb_config_descriptor* cfg;
    if (libusb_get_active_config_descriptor(libusb_get_device(handle), &cfg) != 0)
    {
      libusb_close(handle);
      return EXIT_FAILURE;
    }

    int interface_index = -1;
    for (int i = 0; i < cfg->bNumInterfaces; ++i)
    {
      const libusb_interface* inter = &cfg->interface[i];
      for (int j = 0; j < inter->num_altsetting; ++j)
      {
        const libusb_interface_descriptor* interdesc = &inter->altsetting[j];
        for (int k = 0; k < interdesc->bNumEndpoints; ++k)
        {
          if (interdesc->endpoint[k].bEndpointAddress == mouse.address)
          {
            interface_index = i;
            break;
          }
        }
        if (interface_index != -1) break;
      }
      if (interface_index != -1) break;
    }

    if (interface_index == -1)
    {
      libusb_free_config_descriptor(cfg);
      libusb_close(handle);
      return EXIT_FAILURE;
    }

    if (libusb_claim_interface(handle, interface_index) != 0)
    {
      libusb_free_config_descriptor(cfg);
      libusb_close(handle);
      return EXIT_FAILURE;
    }

    std::vector<unsigned char> buffer(max_length);
    libusb_transfer* transfer = libusb_alloc_transfer(0);
    if (!transfer)
    {
      libusb_release_interface(handle, interface_index);
      libusb_free_config_descriptor(cfg);
      libusb_close(handle);
      return EXIT_FAILURE;
    }

    tdata->transfer = transfer;
    transfer->user_data = tdata;
    libusb_fill_interrupt_transfer(transfer, handle, mouse.address, buffer.data(), buffer.size(), mouse_callback, tdata, 0);

    int err = libusb_submit_transfer(transfer);
    if (err != 0)
    {
      std::cerr << "Error submitting transfer: " << libusb_error_name(err) << std::endl;
      libusb_free_transfer(transfer);
      libusb_release_interface(handle, interface_index);
      libusb_free_config_descriptor(cfg);
      libusb_close(handle);
      return EXIT_FAILURE;
    }

    transfers.push_back(transfer);

    std::cout << "Transfer submitted successfully" << std::endl;

    return EXIT_SUCCESS;
  }

} // USB