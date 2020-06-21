#include <memory>
#include <string.h>

#include "mmaldriver.h"

std::unique_ptr<MMALDriver> mmalDevice(new MMALDriver());

/**************************************************************************************
** Return properties of device.
***************************************************************************************/
void ISGetProperties (const char *dev)
{
	mmalDevice->ISGetProperties(dev);
}
/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	mmalDevice->ISNewSwitch(dev, name, states, names, n);
}
/**************************************************************************************
** Process new text from client
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	mmalDevice->ISNewText(dev, name, texts, names, n);
}
/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	mmalDevice->ISNewNumber(dev, name, values, names, n);
}
/**************************************************************************************
** Process new blob from client
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
	mmalDevice->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}
/**************************************************************************************
** Process snooped property from another driver
***************************************************************************************/
void ISSnoopDevice (XMLEle *root)
{
  INDI_UNUSED(root);
}
