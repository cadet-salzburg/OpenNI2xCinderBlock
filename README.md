OpenNI2xCinderBlock
===================

Cinder Block for new OpenNI 2.x API (Kinect, PrimeSense and other TOF devices) 
	written by Robert Praxmarer, this piece of software is free of use do with it what u want but don't blame me 

Getting Ready
-------------

* Install OpenNI 2.2alpha and NITE 2.0.0.12
* I am not allowed to redistribute NITE2 with this repo, so you need to copy the content of your NITE2 installation /NITE2/redist to OpenNI2xCinderBlock/bin
	if you don't do this the app crashes cause it can't find the NITE2.dll
* If you start the app from within Visual Studio be sure to put the working dir ..\..\bin in the settings, otherwise dlls won't be found

Features
--------
* RGB, Depth, IR Streams supported (just two streams work simultaniously)
* User Tracking / Skeleton Tracking
* Nonblocking
* Recording / Playback
* Multiple Devices (this wrapper manages multiple devices easy for you to startup, get data, ...) 
* Hotplugging of devices, auto restart when connected device gets unconnected and then reconnected (still experimental, needs testing on multiple devices)

Known Issues 
------------

* Issues with Microsoft Kinect SDK and Microsoft Drivers (tested for MS SDK v1.6, and 1.7)

	* Features like mirror, align, reset are not working with MS Kinect SDK, they work fine with a PrimeSense Device
	  You can't install patched avin2 drivers anymore with OpenNI 2.0 

	* When using with Kinect and Windows Kinect drivers just 8bit image information can be retrieved and is shown

* Other issues
	
	* When using multiple devices, usertracker just works for the first device 
		(tested with KINECT SDK and NITE2 for live and playback devices, should behave the same when using two real devices)
	
	* When the UserTracker is enabled you can't mirror the DepthStream

	* You can't mix devices with different drivers (not possible to run an asus with prime sense driver and a kinect with windows driver)

	* OpenNI function setDepthColorSyncEnable(); freezes app with AsusSensor, it seems to be ignored on windows kinect 

	* You can't play the same file with different positions, if you start the same file twice the always run synchronously 

TODO
----
* Auto Restart still problems to fix
* Skeleton (getters for orientations, ...)
* Alternative event based implementation (should be way faster as it's threaded)
* device number vs. uri, rewrite functions to work with unique ids
* Realtime change of resolutions and active streams
* Device Connect/Disconnect Events
* Seperation Wrapper / Cinder Helper Code
* Test if recording of multiple devices is possible simultaniously
* Quiet mode --> switch of error and debug msgs in console
* TinderBox automatically copying OPENNI and NITE REDIST not sure if tinderbox supports that