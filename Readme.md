# Intrusion detection with opencv

This project offers a lightweight intrusion detection usable on devices like the raspberry pi with one of your
home ip cameras.

Since a lot of these cameras offer a rtsp stream despite their dubious subscription models and communication channels,
you can block outbound traffic completely and proxy home surveillance with this application.

## Quick Start

```
make release
sudo make install
```

## Features

* Monitor the rtsp stream of your chinesium grade camera for movements.

* Send EMails to yourself via *ssmtp*.
    * EMails include pictures of the static scene in which movement was detected.
    * EMails with HIGH or LOW alert level are sent. (It avoids spamming your mailbox)
        * HIGH: Movement was detected for a reliable amount of time
        * LOW: Movement or maybe noise was detected for a shorter amount of time

* Start as service (fork)
    * systemd and init.d files are supplied - Service will respawn when killed.

* Save surveillance images of interest in your persistence.

## Dependencies

* [OpenCV](https://github.com/opencv/opencv) - tested with version *3.4.5*
* The package *ssmtp* is setup for your email account

## Commandline Parameters

* `--foreground <true|false>` If false process will daemonize.
* `--run-dir <path>` change working directory after daemonizing.
* `--url <rtsp-stream>` Url to your trusty camera
* `--email <address>` Your email address (ssmtp has to be configured)
* `--storage <path>` Path to store images, triggered on movement and sent by mail
* `-visual <true|false>` True: Visualize camera stream and processed foreground image (Debugging)
* `--scripts <directory>` directory to the installed sendmail script. 

## Example

* ![Static Scene](https://raw.githubusercontent.com/Jierr/intrusion-detection-cv/master/doc/scene.jpg)
* ![Foreground](https://raw.githubusercontent.com/Jierr/intrusion-detection-cv/master/doc/foreground.jpg)
