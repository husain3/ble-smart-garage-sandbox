#include "NimBLEDevice.h"
#include <map>
#include <queue>

// The remote service we wish to connect to.
static BLEUUID authorizedVehicleServiceUUID("AAAA");

// The characteristics of the remote service we are interested in.
static BLEUUID dateTimeCharacteristicUUID("BBBB");
static BLEUUID hashCharacteristicUUID("CCCC");
static BLEUUID aliveTimeCharacteristicUUID("DDDD");

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
		if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(authorizedVehicleServiceUUID))
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
	BLERemoteService* pAuthorizedVehicleService = pClient->getService(authorizedVehicleServiceUUID);
	if (pAuthorizedVehicleService == nullptr) {
		Serial.print("Failed to find our service UUID: ");
		Serial.println(authorizedVehicleServiceUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
    Serial.println(" - Found our service");

	// Obtain a reference to the characteristic in the service of the remote BLE server.
	NimBLERemoteCharacteristic *pDateTimeCharacteristic;
	NimBLERemoteCharacteristic *pHashCharacteristic;
	NimBLERemoteCharacteristic *pAliveTimeCharacteristic;
	if(pAuthorizedVehicleService != nullptr) {
		pDateTimeCharacteristic = pAuthorizedVehicleService->getCharacteristic(dateTimeCharacteristicUUID);
		pHashCharacteristic = pAuthorizedVehicleService->getCharacteristic(hashCharacteristicUUID);
		pAliveTimeCharacteristic = pAuthorizedVehicleService->getCharacteristic(aliveTimeCharacteristicUUID);
	} else {
		return false;
	}

	if (pDateTimeCharacteristic == nullptr || pHashCharacteristic == nullptr || pAliveTimeCharacteristic == nullptr )
	{
		Serial.print("Failed to find all characteristics: ");
		if(pDateTimeCharacteristic == nullptr)
			Serial.println("BBBB");
		if(pHashCharacteristic == nullptr)
			Serial.println("CCCC");
		if(pAliveTimeCharacteristic == nullptr)
			Serial.println("DDDD");

		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our characteristics");

	if(pHashCharacteristic->canNotify()) {
		if(!pHashCharacteristic->subscribe(true, notifyCallback)) {
			pClient->disconnect();
			return false;
		}
		Serial.println(" - Found our characteristics");
	}

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