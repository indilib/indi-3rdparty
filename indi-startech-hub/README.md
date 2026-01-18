# indi-startech-hub (INDI 3rdparty)

StarTech Industrial USB Hub driver (managed hub, serial-controlled).

## Build inside indi-3rdparty
From the repo root:
```bash
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
make -j
sudo make install
```
