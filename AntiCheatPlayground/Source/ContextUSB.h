#ifndef CONTEXT_USB_H
#define CONTEXT_USB_H

#include <libusb.h>
#include <vector>

namespace USB
{

	struct Mouse
	{
		uint16_t vid;
		uint16_t pid;
		uint8_t address;

		Mouse(uint16_t v, uint16_t p, uint8_t a) : vid(v), pid(p), address(a) {}
	};

	struct TransferData
	{
		using CallbackFn = void(*)(unsigned char*, int);
		CallbackFn callback = nullptr;
		libusb_transfer* transfer = nullptr;
	};

	class ContextUSB
	{

	public:

		ContextUSB();
		~ContextUSB();

	private:

		void init();
		void getMice();
		int listenToDevice(Mouse& mouse, unsigned int max_length, TransferData* tdata);

		static void LIBUSB_CALL mouse_callback(libusb_transfer* transfer);

		libusb_context* ctx = nullptr;
		std::vector<Mouse> mice;
		std::vector<libusb_transfer*> transfers;

	};

} // USB

#endif // CONTEXT_USB_H