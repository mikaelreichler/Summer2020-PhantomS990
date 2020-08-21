#include "tools.h"
#include <iostream>
#include <EGrabber.h>
#include <FormatConverter.h>
#include <string>


#include <cstdint>
#include <iostream> 
#include <thread>

uint64_t timeSinceEpochMillisec() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

using namespace Euresys;
using namespace std;


static const uint32_t CARD_IX = 0;
static const uint32_t DEVICE_IX = 0;

int framecount;
string flag;
string path;

static void sample() {
	EGenTL genTL;
	EGrabber<CallbackOnDemand> grabber(genTL, CARD_IX, DEVICE_IX); // create grabber
	FormatConverter converter(genTL);

	grabber.runScript("config.js");


	std::cout << "Set path to folder: ";
	getline(cin, path);
	if (path == "") {
		std::cout << "\nPath cannot be empty, set path again: ";
		getline(cin, path);
	}
	std::cout << "Set amount of frames and press Enter: ";
	std::cin >> framecount;
	std::cout << "Allocating memory.. please wait";
	uint64_t time0 = timeSinceEpochMillisec();
	grabber.reallocBuffers(framecount);
	uint64_t timeMemory = timeSinceEpochMillisec();
	std::cin.ignore();
	std::cout << "\n\nMemory ready (took " << timeMemory - time0 << " ms)";
	std::cout << "\n" << grabber.getString<Euresys::RemoteModule>("Width") << "x"
		<< grabber.getString<Euresys::RemoteModule>("Height") << " at "
		<< grabber.getFloat<Euresys::RemoteModule>("AcquisitionFrameRate") << "fps";
	std::cout << ", acquiring " << framecount << " frames\n";
	std::cout << "\nPress Enter to Trigger\n";
	std::cin.ignore();

	uint64_t time1 = timeSinceEpochMillisec();
	grabber.start(framecount);

	Sleep(1000 * framecount / grabber.getFloat<Euresys::RemoteModule>("AcquisitionFrameRate"));
	std::cout << "Frames Acquired\nTo write to disk, press Enter. To discard, type \"no\"\n";
	getline(cin, flag);

	if (flag == "no")
	{
		grabber.stop();
		return;
	}
	else {

		for (size_t frame = 0; frame < framecount; ++frame) {
			ScopedBuffer buffer(grabber); // wait and get a buffer
			// Note: ScopedBuffer pushes the buffer back to the input queue automatically
			uint8_t* imagePointer = buffer.getInfo<uint8_t*>(gc::BUFFER_INFO_BASE);
			void* ptr = buffer.getInfo<void*>(GenTL::BUFFER_INFO_BASE);
			FormatConverter::Auto bgr(converter, FormatConverter::OutputFormat("BGR8"), imagePointer,
				buffer.getInfo<uint64_t>(gc::BUFFER_INFO_PIXELFORMAT),
				buffer.getInfo<size_t>(gc::BUFFER_INFO_WIDTH),
				buffer.getInfo<size_t>(gc::BUFFER_INFO_DELIVERED_IMAGEHEIGHT),
				buffer.getInfo<size_t>(gc::BUFFER_INFO_SIZE),
				buffer.getInfo<size_t>(ge::BUFFER_INFO_CUSTOM_LINE_PITCH));

			std::cout << "frame: " << frame << "/" << framecount - 1 << std::endl;

			bgr.saveToDisk(std::string(path) + "/frame.NNNNNN.png", frame);
		}

		uint64_t time2 = timeSinceEpochMillisec();

		std::cout << "Time elapsed: " << time2 - time1 << "ms" << std::endl;
		Sleep(2000);
	}
}

int main() {
	try {
		sample();
	}
	catch (const std::exception& e) {
		std::cout << "error: " << e.what() << std::endl;
		std::cout << "\nThis is probably due to invalid camera configuration parameters in the config.js file or other programs using the camera.. check Memento";
		std::cout << "\n\nPress Enter to close";
		std::cin.ignore();
	}
}