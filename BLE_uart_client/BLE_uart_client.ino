/*
  This is based on the BLE_client example code.
*/

#include "BLEDevice.h"

// BLE UART 서비스와 Rx/Tx UUID
static BLEUUID    serviceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID    rxUUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");  //Server 입장에서의 RX이므로 Client는 Tx이다.
static BLEUUID    txUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");  //Server 입장에서의 Tx이므로 Client는 Rx이다.

//연결 및 장치 검색 상태 변수
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;

// Rx 캐릭터리스틱, Client 입장에선 Tx 즉, 여기서는 데이터 전송을 이것으로 한다.
static BLERemoteCharacteristic* pRxCharacteristic;
// Tx 캐릭터리스틱, Client 입장에선 Rx 즉, 여기서는 데이터를 송신할때 사용한다.
static BLERemoteCharacteristic* pTxCharacteristic;
// 연결할 장치의 정보를 저장하는 변수
static BLEAdvertisedDevice* myDevice;

// RX 처리 콜백 함수
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);  // 이게 데이터!
}

// BLE 연결 상태 콜백 함수 
// 연결되었거나 연결이 끊어졌거나
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    doScan = true;
    Serial.println("onDisconnect");
  }
};

//장치와 연결을 시도하는 함수, 여기가 키(KEY)!!!
bool connectToServer() {
    //myDevice는 연결할 장치의 정보가 담긴 변수이다.
    //아래 장치 스캔에서 해당 변수에 정보를 넣을 것이다.    
    Serial.print("Forming a connection to ");
    //BLE 맥주소 표시
    Serial.println(myDevice->getAddress().toString().c_str());
    
    //서버(Pheriphral)과 연결할 클라이언트(Central) 클래스 생성
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    //연결 상태 이벤트를 받기 위한 콜백 함수 설정 
    pClient->setClientCallbacks(new MyClientCallback());

    //드디어 서버(Pheriphral) 장치와 연결시도!!!
    pClient->connect(myDevice);
    Serial.println(" - Connected to server");
    
    //연결이되면 서비스 정보 받기
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    //서비스 정보가 null이라면 문제가 있으니 연결해제하고 종료
    if (pRemoteService == nullptr) {        
      //서비스 UUID를 출력
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;      
    }
    //서버 입장에서의 Tx 캐릭터리스틱 클래스 얻기
    pTxCharacteristic = pRemoteService->getCharacteristic(txUUID);
    if (pTxCharacteristic == nullptr) {
      //마찬가지로 못 가져오면 문제가 있으니 연결해제하고 종료
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(rxUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    // Tx 캐릭터리스틱이 Notify를 지원하는지 확인.
    if(pTxCharacteristic->canNotify()) {    
      //데이터를 받을 콜백함수 등록
      pTxCharacteristic->registerForNotify(notifyCallback);
    }

    // 서버 입장에서의 Rx 캐릭터리스틱 클래스 얻기
    pRxCharacteristic = pRemoteService->getCharacteristic(rxUUID);
    if (pRxCharacteristic == nullptr) {
      //null이면 문제 있음..
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(txUUID.toString().c_str());
      pClient->disconnect();
      return false;
    } 
    // Rx 캐릭터리스틱에는 데이터를 보내야하므로 write 를 지원해야한다. 
    // write를 지원하는지 확인!
    if(!pRxCharacteristic->canWrite()) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(txUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    //여기까지 왔으면 연결 성공했다고 변수 설정
    connected = true;
    return true;
}


// 장치 검색 콜백함수, 새로운 장치가 검색되면 한번씩 들어옴
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");    
    Serial.print(advertisedDevice.haveServiceUUID());
    Serial.print(" , ");
    Serial.println(advertisedDevice.toString().c_str());
    // 어드버타이징 정보에 서비스 UUID가 있는지 확인하고 있다면 BLE UART 서비스 UUID인지 확인
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      //BLE UART 서비스 UUID라면 우리가 찾으려던 녀석!!
      Serial.print("A device to be connected has been found.");
      //검색을 중지하고...
      BLEDevice::getScan()->stop();
      //해당 장치 정보를 myDevice에 저장
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      //연결을 시작!
      doConnect = true;
      doScan = false;
    }
  }
};

//
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  
  //BLE 클래스 초기화
  BLEDevice::init("");
  //스캔 클래스 생성
  BLEScan* pBLEScan = BLEDevice::getScan();
  //장치 검색되면 호출할 콜백함수 등록
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->start(0); //0은 무제한 검색
} 

void loop() {
  //doConnect가 true 연결을 해야함.
  if (doConnect == true) {
    //연결을 시도하는 함수 호출
    if (connectToServer()) {
      //무사히 연결되었다면!
      Serial.println("We are now connected to the BLE Server.");
    } else {
      //연결 실패시
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    //더이상 연결시도 안하기 위해 변수 false로 변경
    doConnect = false;
  }

  //장치가 연결되었다면
  if (connected) {
    //부팅후 현재까지의 시간
    String newValue = "Time since boot: " + String(millis()/1000);    
    Serial.println("Setting new characteristic value to \"" + newValue + "\"");    
    //그대로 서버(Pheriphral)에 보냄.
    pRxCharacteristic->writeValue(newValue.c_str(), newValue.length());
  }else if(doScan){
    //연결이 해제되고 doScan이 true면 스캔 시작
    BLEDevice::getScan()->start(0);
  }

  //1초 대기후 루프 재시작
  delay(1000);
}
