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

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
	Serial.print("Notify callback for characteristic ");
	Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
	Serial.print(" of data length ");
	Serial.println(length);
	Serial.print("data: ");
	Serial.println((char *)pData);
}

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class Monitor : public BLEClientCallbacks
{
public:
	int16_t connection_id;
	int16_t rssi_average = 0;

	/* dBm to distance parameters; How to update distance_factor 1.place the
	 * phone at a known distance (2m, 3m, 5m, 10m) 2.average about 10 RSSI
	 * values for each of these distances, Set distance_factor so that the
	 * calculated distance approaches the actual distances, e.g. at 5m. */
	static constexpr float reference_power = -50;
	static constexpr float distance_factor = 3.5;

	// uint8_t get_value() { return value++; }
	// esp_err_t get_rssi() { return esp_ble_gap_read_rssi(remote_addr); }

	static float get_distance(const int8_t rssi)
	{
		return pow(10, (reference_power - rssi) / (10 * distance_factor));
	}

	void onConnect(BLEClient *pClient)
	{
		connectedDevices[pClient->getConnId()] = pClient;
		Serial.println("onConnect");
		pClient->updateConnParams(120, 120, 0, 60);
	}

	void onDisconnect(BLEClient *pClient)
	{
		connectedDevices.erase(pClient->getConnId());
		Serial.println("onDisconnect");
	}
	/***************** New - Security handled here ********************
	****** Note: these are the same return values as defaults ********/
	uint32_t onPassKeyRequest()
	{
		Serial.println("Client PassKeyRequest");
		return 123456;
	}
	bool onConfirmPIN(uint32_t pass_key)
	{
		Serial.print("The passkey YES/NO number: ");
		Serial.println(pass_key);
		return true;
	}

	void onAuthenticationComplete(ble_gap_conn_desc *desc)
	{
		Serial.println("Starting BLE work!");
	}
	/*******************************************************************/
};

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

	NimBLEClient *pClient = BLEDevice::createClient();
	pClient->setClientCallbacks(new Monitor());
	Serial.println(" - Created client");

	// Connect to the remove BLE Server.
	pClient->connect(currentDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)

	if (pClient->connect(currentDevice))
		Serial.println(" - Connected to server");

	// Obtain a reference to the service we are after in the remote BLE server.
	BLERemoteService *pAuthorizedVehicleService = pClient->getService(authorizedVehicleServiceUUID);
	if (pAuthorizedVehicleService == nullptr)
	{
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
	if (pAuthorizedVehicleService != nullptr)
	{
		pDateTimeCharacteristic = pAuthorizedVehicleService->getCharacteristic(dateTimeCharacteristicUUID);
		pHashCharacteristic = pAuthorizedVehicleService->getCharacteristic(hashCharacteristicUUID);
		pAliveTimeCharacteristic = pAuthorizedVehicleService->getCharacteristic(aliveTimeCharacteristicUUID);
	}
	else
	{
		return false;
	}

	if (pDateTimeCharacteristic == nullptr || pHashCharacteristic == nullptr || pAliveTimeCharacteristic == nullptr)
	{
		Serial.print("Failed to find all characteristics: ");
		if (pDateTimeCharacteristic == nullptr)
			Serial.println("BBBB");
		if (pHashCharacteristic == nullptr)
			Serial.println("CCCC");
		if (pAliveTimeCharacteristic == nullptr)
			Serial.println("DDDD");

		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our characteristics");

	if (pDateTimeCharacteristic->canWrite())
	{
		if (pDateTimeCharacteristic->writeValue<long>(millis(), true))
			Serial.println("Time written to client");
	}

	if (pHashCharacteristic->canNotify())
	{
		if (!pHashCharacteristic->subscribe(true, notifyCallback))
		{
			pClient->disconnect();
			return false;
		}
		Serial.println(" - Found our callback characteristic");
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

long lastWrittenTime = 0;
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

	long currentTime = millis();

	if (currentTime - lastWrittenTime > 1000)
	{
		if (!connectedDevices.empty())
		{

			BLEClient *pClient = connectedDevices.begin()->second;
			NimBLERemoteService *pSvc = pClient->getService("AAAA");
			if (pSvc)
			{
				NimBLERemoteCharacteristic *pChr = pSvc->getCharacteristic("BBBB");
				if (pChr->canWrite())
				{
					if (pChr->writeValue<long>(currentTime))
						Serial.println("Time written to client");
					lastWrittenTime = currentTime;
				}
			}
		}
	}

	delay(1000);
}