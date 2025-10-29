import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_background/flutter_background.dart';
import 'package:http_server/http_server.dart';
import 'package:path_provider/path_provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:dio/dio.dart';

// Global değişkenler UI ile iletişim için
// Global değişkenler - bunları dosyanın başına ekleyin
HttpServer? currentServer;
bool isServerRunning = false;
ValueNotifier<String> serverStatus = ValueNotifier<String>("Başlatılıyor...");
ValueNotifier<List<String>> activityLog = ValueNotifier<List<String>>([]);
ValueNotifier<int> photoCount = ValueNotifier<int>(0);
ValueNotifier<String> lastPhotoTime = ValueNotifier<String>(
  "Henüz fotoğraf alınmadı",
);

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final androidConfig = FlutterBackgroundAndroidConfig(
    notificationTitle: "Fotoğraf Sunucusu",
    notificationText: "ESP32 için HTTP sunucusu çalışıyor...",
    notificationImportance: AndroidNotificationImportance.max,
    enableWifiLock: true,
  );

  final hasPermissions = await FlutterBackground.initialize(
    androidConfig: androidConfig,
  );
  if (hasPermissions) {
    await FlutterBackground.enableBackgroundExecution();
  }

  await Permission.notification.request();

  runApp(const MyApp());
  startServer();
}

Future<void> restartServer() async {
  addToLog("Sunucu yeniden başlatılıyor...");
  startServer();
}

void addToLog(String message) {
  final now = DateTime.now();
  final timeStr =
      "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
  final logEntry = "[$timeStr] $message";

  List<String> currentLog = List.from(activityLog.value);
  currentLog.insert(0, logEntry);
  if (currentLog.length > 50) {
    currentLog = currentLog.sublist(0, 50);
  }
  activityLog.value = currentLog;
}

void startServer() async {
  if (isServerRunning) {
    addToLog("Sunucu zaten çalışıyor");
    return;
  }

  try {
    final directory = await getApplicationDocumentsDirectory();
    currentServer = await HttpServer.bind(InternetAddress.anyIPv4, 8080);
    isServerRunning = true;

    String? serverIP;
    for (var interface in await NetworkInterface.list()) {
      for (var addr in interface.addresses) {
        if (addr.type == InternetAddressType.IPv4 &&
            !addr.isLoopback &&
            !addr.address.startsWith("127")) {
          serverIP = addr.address;
          break;
        }
      }
      if (serverIP != null) break;
    }

    serverIP ??= "0.0.0.0";
    serverStatus.value = "Çalışıyor - $serverIP:${currentServer!.port}";
    addToLog("Sunucu başlatıldı: $serverIP:${currentServer!.port}");

    await for (HttpRequest request in currentServer!) {
      // Mevcut request handling kodu aynı kalacak...
      String clientIP =
          request.connectionInfo?.remoteAddress.address ?? "Bilinmeyen";
      addToLog(
        "İstek alındı: ${request.method} ${request.uri.path} - IP: $clientIP",
      );

      if (request.method == 'POST' && request.uri.path == '/upload') {
        try {
          final content = await HttpBodyHandler.processRequest(request);
          if (content.body is Map && content.body.containsKey('photo')) {
            final photo = content.body['photo'];
            final file = File('${directory.path}/photo.jpg');
            await file.writeAsBytes(photo.content);

            photoCount.value++;
            lastPhotoTime.value = DateTime.now().toString().split('.')[0];
            addToLog("Fotoğraf alındı ve kaydedildi");

            await sendToTelegram();
          }
          request.response.write('OK');
          addToLog("Upload işlemi başarılı");
        } catch (e) {
          addToLog("Upload hatası: $e");
          request.response.statusCode = HttpStatus.internalServerError;
          request.response.write('Error');
        }
      } else {
        request.response.statusCode = HttpStatus.notFound;
        request.response.write('Not Found');
        addToLog("404 - Bulunamadı: ${request.uri.path}");
      }
      await request.response.close();
    }
  } catch (e) {
    isServerRunning = false;
    serverStatus.value = "Hata: $e";
    addToLog("Sunucu hatası: $e");
  }
}

Future<void> sendToTelegram() async {
  try {
    final directory = await getApplicationDocumentsDirectory();
    final file = File('${directory.path}/photo.jpg');

    if (!await file.exists()) {
      addToLog("Telegram gönderim hatası: Dosya bulunamadı");
      return;
    }
    String botToken = dotenv.env['BOT_TOKEN']!;

    //String chatId = "1919049309";
    String chatIdSalih = "1079003671";
    String url = "https://api.telegram.org/bot$botToken/sendPhoto";

    FormData formData = FormData.fromMap({
      "chat_id": chatIdSalih,
      "photo": await MultipartFile.fromFile(file.path, filename: "photo.jpg"),
    });

    addToLog("Telegram'a gönderiliyor...");
    final response = await Dio().post(url, data: formData);

    if (response.statusCode == 200) {
      addToLog("Telegram'a başarıyla gönderildi");
    } else {
      addToLog("Telegram gönderim hatası: ${response.statusCode}");
    }
  } catch (e) {
    addToLog("Telegram hatası: $e");
  }
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ESP32 Fotoğraf Sunucusu',
      theme: ThemeData(primarySwatch: Colors.blue, useMaterial3: true),
      home: const ServerStatusScreen(),
    );
  }
}

class ServerStatusScreen extends StatefulWidget {
  const ServerStatusScreen({super.key});

  @override
  State<ServerStatusScreen> createState() => _ServerStatusScreenState();
}

class _ServerStatusScreenState extends State<ServerStatusScreen> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32 Fotoğraf Sunucusu'),
        backgroundColor: Colors.blue.shade100,
        centerTitle: true,
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Sunucu Durumu
            Card(
              elevation: 4,
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Icon(Icons.computer, color: Colors.blue.shade600),
                        const SizedBox(width: 8),
                        const Text(
                          'Sunucu Durumu',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    ValueListenableBuilder<String>(
                      valueListenable: serverStatus,
                      builder: (context, status, child) {
                        return Container(
                          padding: const EdgeInsets.all(8),
                          decoration: BoxDecoration(
                            color:
                                status.contains("Çalışıyor")
                                    ? Colors.green.shade100
                                    : Colors.red.shade100,
                            borderRadius: BorderRadius.circular(8),
                          ),
                          child: Row(
                            children: [
                              Icon(
                                status.contains("Çalışıyor")
                                    ? Icons.check_circle
                                    : Icons.error,
                                color:
                                    status.contains("Çalışıyor")
                                        ? Colors.green
                                        : Colors.red,
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: Text(
                                  status,
                                  style: TextStyle(
                                    color:
                                        status.contains("Çalışıyor")
                                            ? Colors.green.shade800
                                            : Colors.red.shade800,
                                    fontWeight: FontWeight.w500,
                                  ),
                                ),
                              ),
                            ],
                          ),
                        );
                      },
                    ),
                  ],
                ),
              ),
            ),
            ElevatedButton(
              onPressed: () async {
                try {
                  // Önce mevcut sunucuyu durdur
                  if (currentServer != null) {
                    await currentServer!.close(force: true);
                    addToLog("Mevcut sunucu durduruldu");
                  }

                  // Durumu güncelle
                  serverStatus.value = "Yeniden başlatılıyor...";
                  isServerRunning = false;

                  // Kısa bir bekleme süresi
                  await Future.delayed(Duration(seconds: 1));

                  // Sunucuyu yeniden başlat
                  await restartServer();
                } catch (e) {
                  addToLog("Sunucu yeniden başlatma hatası: $e");
                  serverStatus.value = "Yeniden başlatma hatası: $e";
                }
              },
              child: const Text("Refresh"),
            ),
            const SizedBox(height: 16),

            // İstatistikler
            Row(
              children: [
                Expanded(
                  child: Card(
                    elevation: 4,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        children: [
                          Icon(
                            Icons.photo_camera,
                            size: 32,
                            color: Colors.orange.shade600,
                          ),
                          const SizedBox(height: 8),
                          const Text(
                            'Toplam Fotoğraf',
                            style: TextStyle(fontWeight: FontWeight.bold),
                          ),
                          ValueListenableBuilder<int>(
                            valueListenable: photoCount,
                            builder: (context, count, child) {
                              return Text(
                                count.toString(),
                                style: TextStyle(
                                  fontSize: 24,
                                  fontWeight: FontWeight.bold,
                                  color: Colors.orange.shade600,
                                ),
                              );
                            },
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Card(
                    elevation: 4,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        children: [
                          Icon(
                            Icons.access_time,
                            size: 32,
                            color: Colors.purple.shade600,
                          ),
                          const SizedBox(height: 8),
                          const Text(
                            'Son Fotoğraf',
                            style: TextStyle(fontWeight: FontWeight.bold),
                          ),
                          ValueListenableBuilder<String>(
                            valueListenable: lastPhotoTime,
                            builder: (context, time, child) {
                              return Text(
                                time.split(' ').length > 1
                                    ? time.split(' ')[1].substring(0, 5)
                                    : time,
                                style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: FontWeight.bold,
                                  color: Colors.purple.shade600,
                                ),
                                textAlign: TextAlign.center,
                              );
                            },
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),

            const SizedBox(height: 16),

            // Aktivite Günlüğü
            Expanded(
              child: Card(
                elevation: 4,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          Icon(Icons.list_alt, color: Colors.green.shade600),
                          const SizedBox(width: 8),
                          const Text(
                            'Aktivite Günlüğü',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Expanded(
                        child: ValueListenableBuilder<List<String>>(
                          valueListenable: activityLog,
                          builder: (context, logs, child) {
                            if (logs.isEmpty) {
                              return const Center(
                                child: Text(
                                  'Henüz aktivite bulunmuyor...',
                                  style: TextStyle(
                                    color: Colors.grey,
                                    fontStyle: FontStyle.italic,
                                  ),
                                ),
                              );
                            }
                            return ListView.builder(
                              itemCount: logs.length,
                              itemBuilder: (context, index) {
                                return Container(
                                  padding: const EdgeInsets.symmetric(
                                    vertical: 4,
                                    horizontal: 8,
                                  ),
                                  margin: const EdgeInsets.only(bottom: 2),
                                  decoration: BoxDecoration(
                                    color:
                                        index % 2 == 0
                                            ? Colors.grey.shade50
                                            : Colors.white,
                                    borderRadius: BorderRadius.circular(4),
                                  ),
                                  child: Text(
                                    logs[index],
                                    style: TextStyle(
                                      fontSize: 13,
                                      fontFamily: 'monospace',
                                      color:
                                          logs[index].contains('hata') ||
                                                  logs[index].contains('Hata')
                                              ? Colors.red.shade700
                                              : logs[index].contains(
                                                    'başarı',
                                                  ) ||
                                                  logs[index].contains(
                                                    'gönderildi',
                                                  )
                                              ? Colors.green.shade700
                                              : Colors.black87,
                                    ),
                                  ),
                                );
                              },
                            );
                          },
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
