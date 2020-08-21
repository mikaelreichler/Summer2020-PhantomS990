#include "tools.h"

#include <EGrabber.h>
#include <EGrabbers.h>
#include <FormatConverter.h>

using namespace Euresys;

namespace {

	class SubGrabber;

	class Callbacks {
	public:
		virtual void onNewBufferEvent(SubGrabber& subGrabber, size_t grabberIx, const NewBufferData& data) {};
		virtual void onIoToolboxEvent(SubGrabber& subGrabber, size_t grabberIx, const IoToolboxData& data) {};
		virtual void onCicEvent(SubGrabber& subGrabber, size_t grabberIx, const CicData& data) {};
		virtual void onDataStreamEvent(SubGrabber& subGrabber, size_t grabberIx, const DataStreamData& data) {};
		virtual void onCxpInterfaceEvent(SubGrabber& subGrabber, size_t grabberIx, const CxpInterfaceData& data) {};
		virtual void onDeviceErrorEvent(SubGrabber& subGrabber, size_t grabberIx, const DeviceErrorData& data) {};
		//virtual void onCxpDeviceEvent(SubGrabber& subGrabber, size_t grabberIx, const CxpDeviceData& data) {};
	};

	typedef EGrabber<CallbackOnDemand> EGrabberT;

	class SubGrabber : public EGrabberT {
	public:
		SubGrabber(EGenTL& gentl, int interfaceIndex, int deviceIndex, int dataStreamIndex,
			gc::DEVICE_ACCESS_FLAGS deviceOpenFlags, bool remoteRequired)
			: EGrabberT(gentl, interfaceIndex, deviceIndex, dataStreamIndex, deviceOpenFlags, remoteRequired)
			, callbacks(0)
			, grabberIx(0)
		{}
		void setCallbacks(Callbacks* callbacks, size_t grabberIx) {
			this->callbacks = callbacks;
			this->grabberIx = grabberIx;
		}
	private:
		Callbacks* callbacks;
		size_t grabberIx;
		void onNewBufferEvent(const NewBufferData& data) {
			callbacks->onNewBufferEvent(*this, grabberIx, data);
		}
		void onIoToolboxEvent(const IoToolboxData& data) {
			callbacks->onIoToolboxEvent(*this, grabberIx, data);
		}
		void onCicEvent(const CicData& data) {
			callbacks->onCicEvent(*this, grabberIx, data);
		}
		void onDataStreamEvent(const DataStreamData& data) {
			callbacks->onDataStreamEvent(*this, grabberIx, data);
		}
		void onCxpInterfaceEvent(const CxpInterfaceData& data) {
			callbacks->onCxpInterfaceEvent(*this, grabberIx, data);
		}
		void onDeviceErrorEvent(const DeviceErrorData& data) {
			callbacks->onDeviceErrorEvent(*this, grabberIx, data);
		}
		//void onCxpDeviceEvent(const CxpDeviceData& data) {
		//	callbacks->onCxpDeviceEvent(*this, grabberIx, data);
		//}
	};

	// This sample expects that all connectors of the phantom streamer 16 CXP6 are
	// properly connected to the different grabbers in the right order.
	// In addition, the sample expects that the camera configuration (number of
	// devices exposed by the camera) matches the number of available EGrabber
	// instances the system.

	class SubGrabberManager : private Callbacks {
	public:
		SubGrabberManager(EGenTL& gentl, size_t bufferCount, size_t width, size_t fullHeight, const std::string& pixelFormat)
			: converter(gentl)
			, grabbers(gentl)
			, width(width)
			, height(0)
			, fullHeight(fullHeight)
			, pixelFormat(pixelFormat)
		{
			buffers.reserve(bufferCount);
			size_t pitch = width * gentl.imageGetBytesPerPixel(pixelFormat);
			size_t grabberCount = grabbers.length();
			size_t stripeHeight = 0;
			bool geometry_1X_2YM = false;
			std::vector<std::string> grabberNames;
			for (size_t ix = 0; ix < grabberCount; ++ix) {
				grabberNames.push_back(grabbers[ix]->getString<RemoteModule>("DeviceModelName"));
			}
			if (grabberNames[0] == "Phantom_S990") {
				stripeHeight = 2;
			}
			else {
				stripeHeight = 8 / grabberCount;
			};
			height = fullHeight / grabberCount;
			size_t payloadSize = pitch * fullHeight;
			size_t stripePitch = stripeHeight * grabberCount;
			try {
				for (size_t ix = 0; ix < grabberCount; ++ix) {
					grabbers[ix]->setCallbacks(this, ix);
					// configure camera
					grabbers[ix]->setString<RemoteModule>("Banks", "Banks_ABCD");
					grabbers[ix]->setInteger<RemoteModule>("Width", static_cast<int64_t>(width));
					grabbers[ix]->setInteger<RemoteModule>("Height", static_cast<int64_t>(height));
					grabbers[ix]->setString<RemoteModule>("PixelFormat", pixelFormat);
					grabbers[ix]->setInteger<RemoteModule>("AcquisitionFrameRate", static_cast<int64_t>(938));
					grabbers[ix]->setInteger<RemoteModule>("ExposureTime", static_cast<int64_t>(1000));
					grabbers[ix]->setString<RemoteModule>("TimeStamp", "TSOn");

					// configure stripes on grabber data stream
					grabbers[ix]->setInteger<StreamModule>("LinePitch", pitch);
					grabbers[ix]->setInteger<StreamModule>("LineWidth", pitch);
					grabbers[ix]->setInteger<StreamModule>("StripeHeight", stripeHeight);
					grabbers[ix]->setInteger<StreamModule>("StripePitch", stripePitch);
				}
				while (buffers.size() < bufferCount) {
					uint8_t* base = static_cast<uint8_t*>(malloc(payloadSize));
					if (base == NULL) {
						std::cout << "malloc unsuccesfull";
					}
					buffers.push_back(base);
					for (size_t ix = 0; ix < grabberCount; ++ix) {
						size_t offset = pitch * stripeHeight * ix;
						if (geometry_1X_2YM) {
							grabbers[ix]->announceAndQueue(UserMemory(base, payloadSize));
						}
						else {
							grabbers[ix]->announceAndQueue(UserMemory(base + offset, payloadSize - offset));
						}
					}
				}
			}
			catch (...) {
				cleanup();
				throw;
			}
		}
		virtual ~SubGrabberManager() {
			cleanup();
		}
		void cleanup() {
			for (size_t ix = 0; ix < grabbers.length(); ++ix) {
				grabbers[ix]->reallocBuffers(0);
			}
			for (size_t i = 0; i < buffers.size(); ++i) {
				free(buffers[i]);
			}
			buffers.clear();
		}
		void go(size_t bufferCount) {

			try {
				size_t grabberCount = grabbers.length();
				bool triggerMode = !grabbers[0]->getInteger<RemoteModule>(query::available("SyncImg"));

				Tools::sleepMs(5000);

				if (!triggerMode) {
					// free-run mode with camera firmware older than end of September 2018
					grabbers[0]->setString<RemoteModule>("SyncImg", "External");
				}
				else {
					// triggered mode needs camera firmware from end of September 2018
					grabbers[0]->setString<RemoteModule>("TriggerMode", "TriggerModeOn");   // camera in triggered mode
					grabbers[0]->setString<RemoteModule>("TriggerSource", "SWTRIGGER");     // source of trigger CXP
					grabbers[0]->setString<DeviceModule>("CameraControlMethod", "RC");      // tell grabber 0 to send trigger
					grabbers[0]->setString<DeviceModule>("CycleMinimumPeriod", "20000.0");  // set the trigger rate to 50 Hz
					grabbers[0]->setInteger<DeviceModule>("ExposureReadoutOverlap", 1);     // ignore the first trigger sent by grabber
				}


				// start each grabber in reverse order
				for (size_t ix = 0; ix < grabberCount; ++ix) {
					grabbers[grabberCount - 1 - ix]->start(bufferCount);
				}

				if (!triggerMode) {
					// tell the camera to wait for a trigger
					Tools::sleepMs(2000);
					grabbers[0]->setString<RemoteModule>("SyncImg", "Internal");
				}

				const uint64_t timeout = 2000;
				for (size_t i = 0; i < bufferCount; ++i) {
					// wait for each part of the buffer from each grabber
					for (size_t ix = 0; ix < grabberCount; ++ix) {
						grabbers[ix]->processEvent<NewBufferData>(timeout);
					}
				}

				if (triggerMode) {
					for (size_t ix = 0; ix < grabberCount; ++ix) {
						grabbers[ix]->setString<RemoteModule>("TriggerMode", "TriggerModeOff");
						grabbers[ix]->stop();
					}
				}

				// convert & save merged buffers
				for (size_t i = 0; i < bufferCount; ++i) {
					FormatConverter::Auto rgb(converter, FormatConverter::OutputFormat("RGB8"), buffers[i], pixelFormat, width, fullHeight);
					rgb.saveToDisk(Tools::getEnv("sample-output-path") + "/merged.NNNNNN.png", i);
				}
			}
			catch (...) {
				for (size_t ix = 0; ix < grabbers.length(); ++ix) {
					grabbers[ix]->setString<RemoteModule>("TriggerMode", "TriggerModeOff");
					grabbers[ix]->stop();
				}
				throw;

			}
		}
	private:
		void onNewBufferEvent(SubGrabber& subGrabber, size_t grabberIx, const NewBufferData& data) {
			ScopedBuffer buffer(subGrabber, data);
			uint64_t frameId = buffer.getInfo<uint64_t>(gc::BUFFER_INFO_FRAMEID);
			std::string grabberSdi = grabbers[grabberIx].getSdi();
			Tools::log("grabbers[" + Tools::toString(grabberIx) + "] (" + grabberSdi + ") got frame " + Tools::toString(frameId));
		};

		FormatConverter converter;
		EGrabbers<SubGrabber> grabbers;
		std::vector<uint8_t*> buffers;

		size_t width;
		size_t height;
		size_t fullHeight;
		std::string pixelFormat;
	};

}

static void sample() {
	EGenTL genTL;
	const size_t BUFFER_COUNT = 100;
	SubGrabberManager subGrabberManager(genTL, BUFFER_COUNT, 4096, 2304, "Mono8");
	std::cout << "grabbers configured";
	subGrabberManager.go(BUFFER_COUNT);

}

static Tools::Sample addSample(__FILE__, "Merge buffers from available grabbers connected to phantom streamer 16 CXP6", sample);


int main() {
	std::cout << "starting\n";
	try {
		sample();
	}
	catch (const std::exception& e) {
		std::cout << "error: " << e.what() << std::endl;
	}
}