# libpktriggercord

Shared library wrapper for pktriggercord
by Karl Rees (2020)

The files in the source directory are copied from PkTriggerCord.  Thanks to Andras Salamon for developing PkTriggerCord.  See http://pktriggercord.melda.info/

To update for new versions of PkTriggerCord, copy over the latest versions of the corresponding PkTriggerCord source files found in the src subfolder.  You may need to update the header file include/libpktriggercord.h if any functions in pktriggercord-cli.c have changed.  (TODO: automate extraction of header file directly from pktriggercord-cli.c).

Then run:

cmake .
sudo make install
