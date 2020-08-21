
//Configure these variables

var AcquisitionFrameRate = 2000;
var ExposureTime = 1000000 / AcquisitionFrameRate - 1;
var ShutterMode = "Rolling";
var PixelFormat = "Mono8";
var Width = "1024";
var Height = "1024";

//---------------


var grabber = grabbers[0];
// camera configuration
grabber.RemotePort.set("Banks", "Banks_A");
grabber.RemotePort.set("PixelFormat", PixelFormat);
grabber.RemotePort.set("Width", Width);
grabber.RemotePort.set("Height", Height);
// wait for camera to receive instructions
sleep(0.5);
grabber.RemotePort.set("ShutterMode", ShutterMode);
grabber.RemotePort.set("AcquisitionFrameRate", AcquisitionFrameRate);
// wait for camera to receive instructions
sleep(0.5);
grabber.RemotePort.set("ExposureTime", ExposureTime);
// frame grabber configuration
grabber.DevicePort.set("CameraControlMethod", "RG");
grabber.DevicePort.set("CycleTriggerSource", "Immediate");
grabber.DevicePort.set("CycleMinimumPeriod", 1e6 / AcquisitionFrameRate);

