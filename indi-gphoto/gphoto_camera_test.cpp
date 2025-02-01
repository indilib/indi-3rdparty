#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gphoto2/gphoto2.h>
#include "gphoto_driver.h"

// Global report filename for logging
char g_report_filename[256];

void generate_camera_report(gphoto_driver *gphoto, bool append = false)
{
    FILE *fp = fopen(g_report_filename, append ? "a" : "w");
    if (!fp)
    {
        fprintf(stderr, "Failed to create/open report file\n");
        return;
    }

    if (!append)
    {
        time_t now;
        time(&now);
        fprintf(fp, "Camera Report - Generated on %s", ctime(&now));
        fprintf(fp, "==========================================\n\n");

        // Basic camera info
        fprintf(fp, "Camera Information:\n");
        fprintf(fp, "-------------------\n");
        fprintf(fp, "Manufacturer: %s\n", gphoto_get_manufacturer(gphoto));
        fprintf(fp, "Model: %s\n", gphoto_get_model(gphoto));

        // Supported formats
        int format_count;
        char **formats = gphoto_get_formats(gphoto, &format_count);
        fprintf(fp, "\nSupported Image Formats:\n");
        fprintf(fp, "----------------------\n");
        for (int i = 0; i < format_count; i++)
            fprintf(fp, "- %s\n", formats[i]);

        // ISO capabilities
        int iso_count;
        char **isos = gphoto_get_iso(gphoto, &iso_count);
        fprintf(fp, "\nSupported ISO Values:\n");
        fprintf(fp, "-------------------\n");
        for (int i = 0; i < iso_count; i++)
            fprintf(fp, "- %s\n", isos[i]);

        // Exposure presets
        int preset_count;
        char **presets = gphoto_get_exposure_presets(gphoto, &preset_count);
        fprintf(fp, "\nExposure Presets:\n");
        fprintf(fp, "----------------\n");
        for (int i = 0; i < preset_count; i++)
            fprintf(fp, "- %s\n", presets[i]);

        // Exposure limits
        double min_exp, max_exp;
        gphoto_get_minmax_exposure(gphoto, &min_exp, &max_exp);
        fprintf(fp, "\nExposure Limits:\n");
        fprintf(fp, "---------------\n");
        fprintf(fp, "Minimum: %.3f seconds\n", min_exp);
        fprintf(fp, "Maximum: %.3f seconds\n", max_exp);

        // Capture target
        int capture_target;
        gphoto_get_capture_target(gphoto, &capture_target);
        fprintf(fp, "\nCapture Target:\n");
        fprintf(fp, "--------------\n");
        fprintf(fp, "%s\n", capture_target == GP_UPLOAD_CLIENT ? "Internal Memory" :
                capture_target == GP_UPLOAD_SDCARD ? "Memory Card" : "All");

        // Extended options
        fprintf(fp, "\nExtended Camera Options:\n");
        fprintf(fp, "----------------------\n");
        gphoto_widget_list *iter = gphoto_find_all_widgets(gphoto);
        gphoto_widget *widget;
        while ((widget = gphoto_get_widget_info(gphoto, &iter)))
        {
            fprintf(fp, "Option: %s\n", widget->name);
            fprintf(fp, "  Type: %d\n", widget->type);
            fprintf(fp, "  Read-only: %s\n", widget->readonly ? "Yes" : "No");

            if (widget->type == GP_WIDGET_RANGE)
            {
                fprintf(fp, "  Range: %.2f to %.2f (step: %.2f)\n",
                        widget->min, widget->max, widget->step);
            }
            else if (widget->type == GP_WIDGET_MENU || widget->type == GP_WIDGET_RADIO)
            {
                fprintf(fp, "  Choices:\n");
                for (int i = 0; i < widget->choice_cnt; i++)
                    fprintf(fp, "    - %s\n", widget->choices[i]);
            }
            fprintf(fp, "\n");
        }

        fprintf(fp, "\nCapture Session Log:\n");
        fprintf(fp, "===================\n\n");
    }

    fclose(fp);
}

void log_exposure(const char *message)
{
    FILE *fp = fopen(g_report_filename, "a");
    if (fp)
    {
        time_t now;
        time(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(fp, "[%s] %s\n", timestamp, message);
        fclose(fp);
    }
}

// Exit codes
#define EXIT_SUCCESS 0
#define EXIT_NO_CAMERA 1
#define EXIT_DRIVER_FAILED 2
#define EXIT_INVALID_ARGS 3
#define EXIT_REPORT_ERROR 4

void print_usage()
{
    fprintf(stderr,
            "Usage: gphoto_camera_test [options]\n"
            "Options:\n"
            "  -n NUMBER    Number of exposures (default: 1)\n"
            "  -e SECONDS   Exposure duration in seconds (default: 1)\n"
            "  -i ISO       ISO setting (e.g., 100, 200, 400, etc.)\n"
            "  -f FORMAT    Image format (e.g., RAW, JPG)\n"
            "  -b          Use BULB mode for exposure\n"
            "  -o PREFIX   Output filename prefix (default: image)\n"
            "  -r PATH     Report file path (default: auto-generated)\n"
            "  -h          Show this help message\n"
            "\nExit Codes:\n"
            "  0: Success\n"
            "  1: No camera detected\n"
            "  2: Camera driver initialization failed\n"
            "  3: Invalid arguments\n"
            "  4: Report file error\n");
}

int main(int argc, char *argv[])
{
    int opt;
    int num_exposures = 1;
    float exposure_time = 1.0;
    int iso = -1;
    int format = -1;
    bool use_bulb = false;
    const char *prefix = "image";
    const char *report_path = nullptr;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "n:e:i:f:bo:r:h")) != -1)
    {
        switch (opt)
        {
            case 'n':
                num_exposures = atoi(optarg);
                break;
            case 'e':
                exposure_time = atof(optarg);
                break;
            case 'i':
                iso = atoi(optarg);
                break;
            case 'f':
                // We'll set this later after getting camera formats
                if (strcasecmp(optarg, "RAW") == 0)
                    format = 0;
                else if (strcasecmp(optarg, "JPG") == 0)
                    format = 1;
                break;
            case 'b':
                use_bulb = true;
                break;
            case 'o':
                prefix = optarg;
                break;
            case 'r':
                report_path = optarg;
                break;
            case 'h':
                print_usage();
                return EXIT_SUCCESS;
            default:
                print_usage();
                return EXIT_INVALID_ARGS;
        }
    }

    // Initialize gphoto2 camera
    Camera *camera;
    GPContext *context;

    context = gp_context_new();
    gp_camera_new(&camera);

    // Auto-detect camera
    printf("Detecting camera...\n");
    int ret = gp_camera_init(camera, context);
    if (ret < GP_OK)
    {
        fprintf(stderr, "No camera detected! Error: %d\n", ret);
        return EXIT_NO_CAMERA;
    }

    // Initialize gphoto driver
    gphoto_driver *gphoto = gphoto_open(camera, context, nullptr, nullptr, nullptr);
    if (!gphoto)
    {
        fprintf(stderr, "Failed to initialize camera driver\n");
        return EXIT_DRIVER_FAILED;
    }

    // Set report filename
    if (report_path)
    {
        strncpy(g_report_filename, report_path, sizeof(g_report_filename) - 1);
        g_report_filename[sizeof(g_report_filename) - 1] = '\0';
    }
    else
    {
        time_t now;
        time(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
        snprintf(g_report_filename, sizeof(g_report_filename), "%s_report_%s.txt",
                 gphoto_get_model(gphoto), timestamp);
    }

    // Generate initial camera report
    generate_camera_report(gphoto);
    // Print report path in a format easy to parse (REPORT_PATH:path)
    printf("REPORT_PATH:%s\n", g_report_filename);

    // Set ISO if specified
    if (iso > 0)
    {
        gphoto_set_iso(gphoto, iso);
        printf("ISO set to: %d\n", iso);
    }

    // Set format if specified
    if (format >= 0)
    {
        gphoto_set_format(gphoto, format);
        printf("Format set to: %s\n", format == 0 ? "RAW" : "JPG");
    }

    // Enable BULB mode if requested
    if (use_bulb)
    {
        gphoto_force_bulb(gphoto, true);
        printf("BULB mode enabled\n");
    }

    // Capture loop
    for (int i = 0; i < num_exposures; i++)
    {
        printf("Taking exposure %d of %d...\n", i + 1, num_exposures);

        // Log exposure start
        char msg[256];
        snprintf(msg, sizeof(msg), "Starting exposure %d/%d - %.2f seconds, ISO: %d, Format: %s%s",
                 i + 1, num_exposures, exposure_time, iso,
                 format == 0 ? "RAW" : format == 1 ? "JPG" : "Default",
                 use_bulb ? " (BULB mode)" : "");
        log_exposure(msg);

        // Start exposure
        uint32_t exposure_usec = exposure_time * 1000000;
        ret = gphoto_start_exposure(gphoto, exposure_usec, 0);
        if (ret < 0)
        {
            snprintf(msg, sizeof(msg), "Failed to start exposure %d", i + 1);
            log_exposure(msg);
            fprintf(stderr, "Failed to start exposure\n");
            continue;
        }

        // Wait for exposure completion
        ret = gphoto_read_exposure(gphoto);
        if (ret < 0)
        {
            snprintf(msg, sizeof(msg), "Failed to read exposure %d", i + 1);
            log_exposure(msg);
            fprintf(stderr, "Failed to read exposure\n");
            continue;
        }

        // Get image data
        const char *buffer;
        unsigned long size;
        gphoto_get_buffer(gphoto, &buffer, &size);

        // Generate filename with padding
        char filename[256];
        snprintf(filename, sizeof(filename), "%s_%03d%s", prefix, i + 1, gphoto_get_file_extension(gphoto));

        // Save image
        FILE *fp = fopen(filename, "wb");
        if (fp)
        {
            fwrite(buffer, size, 1, fp);
            fclose(fp);
            snprintf(msg, sizeof(msg), "Saved image to: %s", filename);
            log_exposure(msg);
            printf("%s\n", msg);
        }
        else
        {
            fprintf(stderr, "Failed to save image\n");
        }

        // Free the buffer
        gphoto_free_buffer(gphoto);

        // Add delay between exposures if taking multiple
        if (i < num_exposures - 1)
            sleep(1);
    }

    // Cleanup
    gphoto_close(gphoto);
    gp_camera_exit(camera, context);
    gp_camera_unref(camera);
    gp_context_unref(context);

    // Add capture session summary to report
    char summary[512];
    snprintf(summary, sizeof(summary),
             "\nCapture Session Summary:\n"
             "----------------------\n"
             "Total exposures attempted: %d\n"
             "Exposure time: %.2f seconds\n"
             "ISO: %d\n"
             "Format: %s\n"
             "BULB mode: %s\n"
             "Output prefix: %s\n",
             num_exposures, exposure_time, iso,
             format == 0 ? "RAW" : format == 1 ? "JPG" : "Default",
             use_bulb ? "Yes" : "No",
             prefix);
    log_exposure(summary);

    return EXIT_SUCCESS;
}
