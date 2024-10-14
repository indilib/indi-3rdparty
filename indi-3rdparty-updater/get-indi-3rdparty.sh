#!/bin/bash

# GitHub API variables
GITHUB_API="https://api.github.com"
REPO_OWNER="amprebonne"
REPO_NAME="indi-3rdparty"
GITHUB_TOKEN="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"

# Set API endpoint and headers
ENDPOINT="/repos/${REPO_OWNER}/${REPO_NAME}/contents/"
HEADER="Authorization: Bearer ${GITHUB_TOKEN}"
ACCEPT="Accept: application/vnd.github.v3+json"

# Output file name
OUTPUT_FILE="drivers_info.json"

# Use curl to access the repository and store directory names and hashes
curl -X GET \
  ${GITHUB_API}${ENDPOINT} \
  -H "${HEADER}" \
  -H "${ACCEPT}" \
  | jq -r '.[] | select(.type == "dir" and (.name | test("^indi-.*"))) | {name: .name, sha: .sha}' \
  | jq -s '.' \
  > ${OUTPUT_FILE}


# iterate through driver directories to find driver version file
# read driver version and store in JSON file


# Check for GitHub repository errors
REPO_EXISTS=$(curl -X GET \
  ${GITHUB_API}/repos/${REPO_OWNER}/${REPO_NAME} \
  -H "${HEADER}" \
  -H "${ACCEPT}" \
  -f -s \
  > /dev/null; echo $?)
if [ $REPO_EXISTS -ne 0 ]; then
  handle_error 1 "GitHub repository error: repository not found or inaccessible"
fi

# Check if curl was successful
response=$(curl -X GET -H "${HEADER}" -H "${ACCEPT}" "${GITHUB_API}${ENDPOINT}")
if [ $? -ne 0 ]; then
  echo "Error: Failed to fetch data from GitHub."
  exit 1
fi

# Check if the response contains valid JSON
if ! echo "$response" | jq empty; then
  echo "Error: Invalid JSON response."
  exit 1
fi

# Check if output file was created and is not empty
if [ ! -s ${OUTPUT_FILE} ]; then
  echo "Error: No matching directories found or output file is empty."
  exit 1
fi
