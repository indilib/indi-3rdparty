# libpktriggercord

Shared library wrapper for pktriggercord
by Karl Rees (2020)

Thanks to Andras Salamon for developing PkTriggerCord.  See http://pktriggercord.melda.info/

To update for new versions of PkTriggerCord, place the latest version of PkTriggerCord in the src folder.  You may need to update the header file include/libpktriggercord.h if any functions in pktriggercord-cli.c have changed.  (TODO: automate extraction of header file directly from pktriggercord-cli.c).

Then run:

cmake .
sudo make install
