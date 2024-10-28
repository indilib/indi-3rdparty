#!/usr/bin/python3

import requests

# access and download debian astro packages webpage
# URL for Debian package information
url = "https://qa.debian.org/developer.php?email=debian-astro-maintainers%40lists.alioth.debian.org"

# Send GET request
response = requests.get(url)

# Save HTML content to file
with open("debian-astro-packages.html", "w") as file:
    file.write(response.text)
