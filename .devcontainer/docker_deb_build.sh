#!/bin/bash

set -e

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
project_dir="${script_dir}/.."

image_name="microstrain/inertial_connect:dev"

docker build \
    -t "${image_name}" \
    --build-arg UBUNTU_VERSION="22.04" \
    -f "${script_dir}/Dockerfile.dev" \
    "${project_dir}"

# Different flags for run depending on if we are running in jenkins or not
if [ "${ISHUDSONBUILD}" != "True" ]; then
  docker_it_flags="-it"
else
  configure_flags="\
    -DIC_BUILD_NUMBER=${BUILD_NUMBER} \
    -DIC_BRANCH=${BRANCH_NAME}"
fi

# Build the project
docker run \
  --rm \
  ${docker_it_flags} \
  --entrypoint="/bin/bash" \
  -v "${project_dir}:/home/microstrain/InertialConnect" \
  -w "/home/microstrain/InertialConnect" \
  --user="microstrain" \
  "${image_name}" -c " \
    cmake . \
      --preset linux-release \
      ${configure_flags} \
      ; \
    cmake --build ./build -j$(nproc); \
    cmake --build ./build --target package; \
  "
