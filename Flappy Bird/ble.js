const serviceUUID = "19b10000-e8f2-537e-4f6c-d104768a1214";
const gestureUUID = "19b10001-e8f2-537e-4f6c-d104768a1214";

let bleDevice;
let gestureCharacteristic;

async function connect() {
  try {
    bleDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [serviceUUID] }],
      optionalServices: [gestureUUID]
    });

    console.log("Device found:", bleDevice.name);

    bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

    console.log("Connecting");
    const server = await bleDevice.gatt.connect();
    console.log("GATT connected");
    console.log("getting primary service");

    const service = await server.getPrimaryService(serviceUUID);
    
    console.log("primary service connected");
    
    gestureCharacteristic = await service.getCharacteristic(gestureUUID);
    
    console.log("Gesture characteristic found");
    await gestureCharacteristic.startNotifications();
    gestureCharacteristic.addEventListener('characteristicvaluechanged', handleGestureValues);
  
    console.log('Connected to ' + bleDevice.name);
  } catch (error) {
    console.error("Error:", error);
  }

}

function startNotifications() {
  gestureCharacteristic.addEventListener("characteristicvaluechanged", handleGestureValues);
  gestureCharacteristic.startNotifications();
  console.log("Notifications started.");
}

function handleGestureValues(event) {
  const data = new Float32Array(event.target.value.buffer);

  console.log(data);
  var dir = "0";
  switch(data[0]) {
    case 0: dir = "Up";
            bird.flap();
            break;
    case 1: dir = "Down";
            break;
    case 2: dir = "Left";
            break;
    case 3: dir = "Right";
            break;
    default: dir = "Unknown";
    break;
  }
}

window.addEventListener('beforeunload', () => {
  if (bleDevice && bleDevice.gatt.connected) {
    bleDevice.gatt.disconnect();
  }
});

async function onDisconnected(event) {
  console.log('Disconnected from ' + bleDevice.name);
  await bleDevice.gatt.disconnect()
  connect();
}
