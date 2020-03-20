# Particle Web Server
An attempt at an unbloated, minimalist web server.

## Building
Run these commands to create a HTTP (unsecured) web server binary:
```sh
make  # 'make' will create the binary 'server'.
```
To create a secure HTTPS server run these commands:
```sh
patch < patches/libressl.diff       # Apply the LibreSSL patch.
edit config.h                       # Edit the configuration (enter your certificate details in here).
make                                # 'make' will create the binary 'server'.
```
You will need to change the 'TLSKeyFile' and 'TLSCertFile' constants to your certificate paths. See config.h for more details.

## Quick Start
Starting a HTTP server:
```sh
./server  # Start the server on the default port 80. 
```
Starting a HTTPS server:
```sh
./server -p 443  # Start the server on port 443.
```

## Command Line Options:
| Option | Default | Description
| - | - | -
| -d | /var/www/html/ | Sets the directory of the WWW files.
| -h | N/A | Displays a help message
| -l | Disabled | Enable request logging.
| -p | 80 | Sets the port of the server.
| -t | 20 | Sets the maximum thread count.
| -u | Enabled | Disable the default index.html creation.
| -w | Enabled | Disable all warnings.

## License
This software is licensed under the permissive OpenBSD license. See the `COPYING` file for more information.

