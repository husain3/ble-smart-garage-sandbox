#include "NimBLEDevice.h"
#include <map>
#include <queue>

// The remote service we wish to connect to.
static BLEUUID serviceUUID("BAAD");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("F00D");

NimBLEScan *pBLEScan;

std::queue<NimBLEAddress> deviceToConnect;
std::map<uint16_t, BLEClient *> connectedDevices;

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
	Serial.print("Notify callback for characteristic ");
	Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
	Serial.print(" of data length ");
	Serial.println(length);
	Serial.print("data: ");
	Serial.println((char*)pData);
}

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
	void onResult(NimBLEAdvertisedDevice *advertisedDevice)
	{
		Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
		if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID))
		{
			deviceToConnect.push(advertisedDevice->getAddress());
		}
	}
};

bool connectToServer()
{
	NimBLEAddress currentDevice = deviceToConnect.front();
	deviceToConnect.pop();

	Serial.print("Forming a connection to ");
	Serial.println(currentDevice.toString().c_str());

	BLEClient *pClient = BLEDevice::createClient();
	Serial.println(" - Created client");

	// Connect to the remove BLE Server.
	pClient->connect(currentDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)

	if(pClient->connect(currentDevice))
		Serial.println(" - Connected to server");

	// Obtain a reference to the service we are after in the remote BLE server.
	BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
	if (pRemoteService == nullptr) {
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
    Serial.println(" - Found our service");

	// Obtain a reference to the characteristic in the service of the remote BLE server.
	NimBLERemoteCharacteristic *pRemoteCharacteristic;
	if(pRemoteService != nullptr) {
		pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
	} else {
		return false;
	}

	if (pRemoteCharacteristic == nullptr)
	{
		Serial.print("Failed to find our characteristic UUID: ");
		Serial.println(charUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our characteristic");

	if(pRemoteCharacteristic->canNotify()) {
		if(!pRemoteCharacteristic->subscribe(true, notifyCallback)) {
			pClient->disconnect();
			return false;
		}
		Serial.println(" - Found our characteristic");
	}

	connectedDevices[pClient->getConnId()] = pClient;
	return true;
}

void setup()
{
	Serial.begin(115200);
	Serial.println("Scanning...");

	NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);

	NimBLEDevice::init("");

	pBLEScan = NimBLEDevice::getScan(); // create new scan
	// Set the callback for when devices are discovered, no duplicates.
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
	pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
	pBLEScan->setInterval(97);	   // How often the scan occurs / switches channels; in milliseconds,
	pBLEScan->setWindow(37);	   // How long to scan during the interval; in milliseconds.
	pBLEScan->setMaxResults(0);	   // do not store the scan results, use callback only.
}

void loop()
{
	if (!deviceToConnect.empty())
	{
		if (connectToServer())
		{
			Serial.println("We are now connected to the BLE Server.");
		}
		else
		{
			Serial.println("We have failed to connect to the server; there is nothin more we will do.");
		}
	}

	// If an error occurs that stops the scan, it will be restarted here.
	if (pBLEScan->isScanning() == false)
	{
		// Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
		pBLEScan->start(0, nullptr, false);
	}

	delay(1000);
}