/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_hello.cpp - libcamera "hello world" app.
 */

#include <chrono>

#include "core/rpicam_app.hpp"
#include "core/still_options.hpp"

using namespace std::placeholders;

// The main event loop for the application.

class RPiCamTestApp : public RPiCamApp
{
public:
    RPiCamTestApp() : RPiCamApp(std::make_unique<StillOptions>()) {}

    StillOptions *GetOptions() const
    {
        return static_cast<StillOptions *>(options_.get());
    }
};

static void event_loop()
{
    RPiCamTestApp app;
	auto options = app.GetOptions();

    options->camera = 0;
    options->nopreview = true;
    options->immediate = false;
    options->quality = 100;
    options->restart = false;
    options->thumb_quality = 0;
    options->denoise = "cdn_off";
	
	app.OpenCamera();
	app.ConfigureStill(RPiCamApp::FLAG_STILL_RAW);
	app.StartCamera();

    std::this_thread::sleep_for(1s);
	
    app.StopCamera();
    app.Teardown();
    app.CloseCamera();
}

int main(int argc, char *argv[])
{
	for (int i=0; i < 3; i++)
        event_loop();
}
