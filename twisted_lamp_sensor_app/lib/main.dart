import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'dart:typed_data';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Twisted Lamp Sensor',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const MyHomePage(title: 'Twisted Lamp Sensor'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({Key? key, required this.title}) : super(key: key);

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  StreamSubscription? _subscription;
  StreamSubscription<ConnectionStateUpdate>? _connection;

  var serviceId = Uuid.parse('b3c0060a-2fb7-11ed-a261-0242ac120002');
  var characteristicIdTemp = Uuid.parse('be36cbfa-2fb7-15ed-a261-0242ac120002');
  var characteristicIdHum = Uuid.parse('be36cbfa-2fb7-11ed-a261-0242ac120002');
  var characteristicIdECO2 = Uuid.parse('f908154a-2fb7-11ed-a261-0242ac120002');
  var characteristicIdTVOC = Uuid.parse('0be3e98c-2fb8-11ed-a261-0242ac120002');

  QualifiedCharacteristic? charTemp;
  QualifiedCharacteristic? charHum;
  QualifiedCharacteristic? charTVOC;
  QualifiedCharacteristic? charECO2;

  final bleManager = FlutterReactiveBle();
  var lastTemp = 0.0;
  var lastHum = 0.0;
  var lastTVOC = 0;
  var lastECO2 = 0;

  @override
  void initState() {
    super.initState();

    bleManager.statusStream.listen((status) {
      print("STATUS: $status");
      if (status == BleStatus.ready) initBle();
    });
  }

  @override
  void dispose() {
    _subscription?.cancel();
    _connection?.cancel();
    super.dispose();
  }

  Future<void> stopScan() async {
    print('HF: stopping BLE scan');
    await _subscription?.cancel();
    _subscription = null;
  }

  void initBle() {
    _subscription?.cancel();
    _subscription = bleManager.scanForDevices(
        withServices: [serviceId],
        scanMode: ScanMode.lowLatency).listen((device) {
      print("SCAN FOUND: ${device.name}");
      stopScan();

      _connection = bleManager
          .connectToDevice(
        id: device.id,
        servicesWithCharacteristicsToDiscover: {
          serviceId: [
            characteristicIdTemp,
            characteristicIdHum,
            characteristicIdTVOC,
            characteristicIdECO2
          ]
        },
        connectionTimeout: const Duration(seconds: 2),
      )
          .listen((connectionState) {
        print("CONNECTING: $connectionState");
        if (connectionState.connectionState ==
            DeviceConnectionState.connected) {
          setState(() {
            charTemp = QualifiedCharacteristic(
                serviceId: serviceId,
                characteristicId: characteristicIdTemp,
                deviceId: device.id);

            charHum = QualifiedCharacteristic(
                serviceId: serviceId,
                characteristicId: characteristicIdHum,
                deviceId: device.id);

            charTVOC = QualifiedCharacteristic(
                serviceId: serviceId,
                characteristicId: characteristicIdTVOC,
                deviceId: device.id);

            charECO2 = QualifiedCharacteristic(
                serviceId: serviceId,
                characteristicId: characteristicIdECO2,
                deviceId: device.id);
          });
          someStuff();
        } else {
          print("NOT CONNECTED");
          initBle(); // try to connect to the device until it's in range.
        }
      }, onError: (Object error) {
        print("error on connect: $error");
      });
    }, onError: (obj, stack) {
      print('AN ERROR WHILE SCANNING:\r$obj\r$stack');
    });
  }

  void someStuff() async {
    var tmpTemp, tmpHum, tmpTVOC, tmpECO2;

    // Temperature
    final stream = bleManager.subscribeToCharacteristic(charTemp!);
    await for (final data in stream) {
      tmpTemp = ByteData.view(Uint8List.fromList(data).buffer)
          .getFloat32(0, Endian.little);
      print("GOT TEMP DATA: $tmpTemp");
    }

    // Humidity
    final stream1 = bleManager.subscribeToCharacteristic(charHum!);
    await for (final data in stream1) {
      tmpHum = ByteData.view(Uint8List.fromList(data).buffer)
          .getFloat32(0, Endian.little);
      print("GOT HUM DATA: $tmpHum");
    }

    // TVOC
    final stream2 = bleManager.subscribeToCharacteristic(charTVOC!);
    await for (final data in stream2) {
      tmpTVOC = ByteData.view(Uint8List.fromList(data).buffer)
          .getInt32(0, Endian.little);
      print("GOT TVOC DATA: $tmpTVOC");
    }

    // ECO2
    final stream3 = bleManager.subscribeToCharacteristic(charECO2!);
    await for (final data in stream3) {
      tmpECO2 = ByteData.view(Uint8List.fromList(data).buffer)
          .getInt32(0, Endian.little);
      print("GOT ECO2 DATA: $tmpECO2");
    }
    setState(() {
      lastTemp = tmpTemp;
      lastHum = tmpHum;
      lastECO2 = tmpECO2;
      lastTVOC = tmpTVOC;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (charTemp == null ||
        charHum == null ||
        charECO2 == null ||
        charTVOC == null) {
      print("IS NULLL");

      return Column(
        mainAxisAlignment: MainAxisAlignment.center,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [CircularProgressIndicator()],
      );
    }

    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: StreamBuilder<List<int>>(
          stream: bleManager.subscribeToCharacteristic(charTemp!),
          builder: (BuildContext context, AsyncSnapshot<List<int>> snapshot) {
            if (snapshot.hasError) {
              print(snapshot.error);
              return Center(child: Text('${snapshot.error}'));
            }

            if (snapshot.connectionState == ConnectionState.waiting &&
                lastTemp == 0)
              return Center(child: CircularProgressIndicator());

            // List<int> data = snapshot.requireData;
            // var dataInt = ByteData.view(Uint8List.fromList(data).buffer)
            //     .getUint32(0, Endian.little);

            return Center(
                child: Text(
                    'Temperature : ${lastTemp.toStringAsFixed(2)}\nHumidity : ${lastHum.toStringAsFixed(2)}\nTVOC : $lastTVOC\nECO2 : $lastECO2',
                    style: TextStyle(fontSize: 56)));
          }),
    );
  }
}
