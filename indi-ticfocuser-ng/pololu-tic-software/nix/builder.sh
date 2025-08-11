source $setup

cmake-cross $src \
  -DBUILD_SHARED_LIBS=false \
  -DCMAKE_INSTALL_PREFIX=$out

make
make install

$host-strip $out/bin/*
rm -r $out/include $out/lib
cp version.txt $out/

if [ $os = "linux" ]; then
  cp $dejavu/ttf/DejaVuSans.ttf $out/bin/
fi
