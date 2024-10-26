#!/usr/bin/python3

import requests
from lxml import html

url = 'https://tracker.debian.org/pkg/indi-apogee'

response = requests.get(url)

tree = html.fromstring(response.text)

# Extract div with class "container-fluid main"
div = tree.xpath('//div[@class="container-fluid main"]')[0]

# Extract all list items
list_items = div.xpath('.//ul/li')

# Initialize dictionaries to store extracted data
source = {}
version = {}

# Iterate through list items
for item in list_items:
    key = item.xpath('.//span[@class="list-item-key"]/b/text()')[0].strip().lower()
    value = item.xpath('.//span[@class="list-item-key"]/following-sibling::text()')[0].strip()

    # Extract source and version
    if key == 'source:':
        source['source'] = value
    elif key == 'version:':
        version['version'] = value

# Print extracted list items
for i, item in enumerate(list_items):
    key_xpath = './/span[@class="list-item-key"]/b/text()'
    value_xpath = './/span[@class="list-item-key"]/following-sibling::text()'

    key = item.xpath(key_xpath)[0].strip()
    value = item.xpath(value_xpath)[0].strip()

    print(f"{i+1}. {key} {value}")

# Print extracted source and version
print("\nSource:", source['source'])
print("Version:", version['version'])
