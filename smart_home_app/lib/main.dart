import 'dart:io';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:image_picker/image_picker.dart';
import 'dart:convert';


import 'package:shared_preferences/shared_preferences.dart';

// Main entry point for the application
void main() {
  runApp(SmartHomeApp());
}

class SmartHomeApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Smart Home Control',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        visualDensity: VisualDensity.adaptivePlatformDensity,
        brightness: Brightness.light,
      ),
      darkTheme: ThemeData(
        primarySwatch: Colors.blue,
        visualDensity: VisualDensity.adaptivePlatformDensity,
        brightness: Brightness.dark,
      ),
      themeMode: ThemeMode.system,
      home: LoginScreen(),
      routes: {
        '/dashboard': (context) => DashboardScreen(),
        '/add_device': (context) => AddDeviceScreen(),
        '/device_details': (context) => DeviceDetailsScreen(),
        '/camera_setup': (context) => CameraSetupScreen(),
        '/settings': (context) => SettingsScreen(),
        '/signup':(context)=>SignupScreen()
      },
    );
  }
}

// Login screen for user authentication
class LoginScreen extends StatefulWidget {
  @override
  _LoginScreenState createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final TextEditingController _usernameController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  final _formKey = GlobalKey<FormState>();
  bool _isLoading = false;
  String _errorMessage = '';

  Future<void> _login() async {
    if (_formKey.currentState!.validate()) {
      setState(() {
        _isLoading = true;
        _errorMessage = '';
      });

      try {
        final response = await http.post(
          Uri.parse('https://well-scallop-cybergenii-075601d4.koyeb.app/auth/login'),
          headers: {'Content-Type': 'application/json'},
          body: jsonEncode({
            'username': _usernameController.text,
            'password': _passwordController.text,
          }),
        );

        final responseData = jsonDecode(response.body);
print(responseData.toString());
        if (response.statusCode == 200) {
          final prefs = await SharedPreferences.getInstance();
          await prefs.setString('token', responseData['access_token']);
          await prefs.setString('username', _usernameController.text);
          
          Navigator.pushReplacementNamed(context, '/dashboard');
        } else {
          setState(() {
            _errorMessage = responseData['message'] ?? 'Authentication failed';
          });
        }
      } catch (e) {
        setState(() {
          _errorMessage = 'Network error: $e';
        });
      } finally {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  @override

  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Smart Home Login'),
      ),
      body: Center(
        child: SingleChildScrollView(
          padding: EdgeInsets.all(16.0),
          child: Form(
            key: _formKey,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Image.asset(
                  'assets/images/logo.jpg',
                  height: 120,
                ),
                SizedBox(height: 32.0),
                TextFormField(
                  controller: _usernameController,
                  decoration: InputDecoration(
                    labelText: 'Username',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.person),
                  ),
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter your username';
                    }
                    return null;
                  },
                ),
                SizedBox(height: 16.0),
                TextFormField(
                  controller: _passwordController,
                  decoration: InputDecoration(
                    labelText: 'Password',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.lock),
                  ),
                  obscureText: false,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter your password';
                    }
                    return null;
                  },
                ),
                SizedBox(height: 24.0),
                if (_errorMessage.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 16.0),
                    child: Text(
                      _errorMessage,
                      style: TextStyle(color: Colors.red),
                    ),
                  ),
                ElevatedButton(
                  onPressed: _isLoading ? null : _login,
                  child: _isLoading
                      ? CircularProgressIndicator(
                          color: Colors.white,
                        )
                      : Text('Login'),
                  style: ElevatedButton.styleFrom(
                    minimumSize: Size(double.infinity, 48),
                  ),
                ),
                SizedBox(height: 16.0),
                TextButton(
                  onPressed: () {
                    Navigator.pushNamed(context, '/signup');
                  },
                  child: Text('Don\'t have an account? Sign up'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

}

class SignupScreen extends StatefulWidget {
  const SignupScreen({super.key});

  @override
  _SignupScreenState createState() => _SignupScreenState();
}

class _SignupScreenState extends State<SignupScreen> {
  final TextEditingController _usernameController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  final TextEditingController _confirmPasswordController = TextEditingController();
  final TextEditingController _emailController = TextEditingController();
  final _formKey = GlobalKey<FormState>();
  bool _isLoading = false;
  String _errorMessage = '';
  File? _profileImage;

  Future<void> _pickImage() async {
    final ImagePicker picker = ImagePicker();
    final XFile? image = await picker.pickImage(source: ImageSource.gallery);
    
    if (image != null) {
      setState(() {
        _profileImage = File(image.path);
      });
    }
  }

  Future<void> _signup() async {
    if (_formKey.currentState!.validate()) {
      if (_passwordController.text != _confirmPasswordController.text) {
        setState(() {
          _errorMessage = 'Passwords do not match';
        });
        return;
      }

      setState(() {
        _isLoading = true;
        _errorMessage = '';
      });

      try {
        // Create multipart request for profile image upload
        var request = http.MultipartRequest(
          'POST',
          Uri.parse('https://well-scallop-cybergenii-075601d4.koyeb.app/auth/signup'),
        );
        
        // Add text fields
        request.fields['username'] = _usernameController.text;
        request.fields['password'] = _passwordController.text;
        request.fields['full_name'] = _emailController.text;
        
        // Add file if selected
        if (_profileImage != null) {
          request.files.add(await http.MultipartFile.fromPath(
            'profile_image',
            _profileImage!.path,
          ));
        }
        
        // Send the request
        var streamedResponse = await request.send();
        var response = await http.Response.fromStream(streamedResponse);
        
        final responseData = jsonDecode(response.body);

        if (response.statusCode == 201) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Account created successfully!')),
          );
          Navigator.pushReplacementNamed(context, '/login');
        } else {
          setState(() {
            _errorMessage = responseData['detail'] ?? 'Signup failed';
          });
        }
      } catch (e) {
        setState(() {
          _errorMessage = 'Network error: $e';
        });
      } finally {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Create Account'),
      ),
      body: Center(
        child: SingleChildScrollView(
          padding: EdgeInsets.all(16.0),
          child: Form(
            key: _formKey,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                GestureDetector(
                  onTap: _pickImage,
                  child: CircleAvatar(
                    radius: 50,
                    backgroundImage: _profileImage != null 
                        ? FileImage(_profileImage!) 
                        : AssetImage('assets/images/pol.png') as ImageProvider,
                    child: _profileImage == null
                      ? Icon(Icons.add_a_photo, size: 30, color: Colors.white70)
                      : null,
                  ),
                ),
                SizedBox(height: 8.0),
                Text('Tap to add profile photo', style: TextStyle(color: Colors.grey)),
                SizedBox(height: 24.0),
                TextFormField(
                  controller: _usernameController,
                  decoration: InputDecoration(
                    labelText: 'Username',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.person),
                  ),
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter a username';
                    }
                    return null;
                  },
                ),
                SizedBox(height: 16.0),
                TextFormField(
                  controller: _emailController,
                  decoration: InputDecoration(
                    labelText: 'full name',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.email),
                  ),
                  keyboardType: TextInputType.emailAddress,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter your name';
                    }
                   
                    return null;
                  },
                ),
                SizedBox(height: 16.0),
                TextFormField(
                  controller: _passwordController,
                  decoration: InputDecoration(
                    labelText: 'Password',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.lock),
                  ),
                  obscureText: true,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter a password';
                    }
                    if (value.length < 8) {
                      return 'Password must be at least 8 characters';
                    }
                    return null;
                  },
                ),
                SizedBox(height: 16.0),
                TextFormField(
                  controller: _confirmPasswordController,
                  decoration: InputDecoration(
                    labelText: 'Confirm Password',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.lock_outline),
                  ),
                  obscureText: true,
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please confirm your password';
                    }
                    return null;
                  },
                ),
                SizedBox(height: 24.0),
                if (_errorMessage.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 16.0),
                    child: Text(
                      _errorMessage,
                      style: TextStyle(color: Colors.red),
                    ),
                  ),
                ElevatedButton(
                  onPressed: _isLoading ? null : _signup,
                  child: _isLoading
                      ? CircularProgressIndicator(
                          color: Colors.white,
                        )
                      : Text('Create Account'),
                  style: ElevatedButton.styleFrom(
                    minimumSize: Size(double.infinity, 48),
                  ),
                ),
                SizedBox(height: 16.0),
                TextButton(
                  onPressed: () {
                    Navigator.pushReplacementNamed(context, '/');
                  },
                  child: Text('Already have an account? Login'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
// Dashboard screen showing all devices
class DashboardScreen extends StatefulWidget {
  @override
  _DashboardScreenState createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  List<DeviceModel> _devices = [];
  bool _isLoading = true;
  String _errorMessage = '';

  @override
  void initState() {
    super.initState();
    _fetchDevices();
  }

  Future<void> _fetchDevices() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final token = prefs.getString('  token');

      final response = await http.get(
        Uri.parse('https://well-scallop-cybergenii-075601d4.koyeb.app/api/devices'),
        headers: {
          'Authorization': 'Bearer $token',
        },
      );

      if (response.statusCode == 200) {
        final List<dynamic> devicesJson = jsonDecode(response.body);
        setState(() {
          _devices = devicesJson
              .map((device) => DeviceModel.fromJson(device))
              .toList();
          _isLoading = false;
        });
      } else {
        setState(() {
          _errorMessage = 'Failed to load devices';
          _isLoading = false;
        });
      }
    } catch (e) {
      setState(() {
        _errorMessage = 'Network error: $e';
        _isLoading = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Smart Home Dashboard'),
        actions: [
          IconButton(
            icon: Icon(Icons.settings),
            onPressed: () {
              Navigator.pushNamed(context, '/settings');
            },
          ),
        ],
      ),
      body: _isLoading
          ? Center(child: CircularProgressIndicator())
          : _errorMessage.isNotEmpty
              ? Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Text(
                        _errorMessage,
                        style: TextStyle(color: Colors.red),
                      ),
                      SizedBox(height: 16),
                      ElevatedButton(
                        onPressed: _fetchDevices,
                        child: Text('Retry'),
                      ),
                    ],
                  ),
                )
              : _devices.isEmpty
                  ? Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Text('No devices found'),
                          SizedBox(height: 16),
                          ElevatedButton(
                            onPressed: () {
                              Navigator.pushNamed(context, '/add_device');
                            },
                            child: Text('Add Device'),
                          ),
                        ],
                      ),
                    )
                  : RefreshIndicator(
                      onRefresh: _fetchDevices,
                      child: GridView.builder(
                        padding: EdgeInsets.all(16),
                        gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: 2,
                          childAspectRatio: 1.0,
                          crossAxisSpacing: 16,
                          mainAxisSpacing: 16,
                        ),
                        itemCount: _devices.length,
                        itemBuilder: (context, index) {
                          final device = _devices[index];
                          return _buildDeviceCard(device);
                        },
                      ),
                    ),
      floatingActionButton: FloatingActionButton(
        onPressed: () {
          Navigator.pushNamed(context, '/add_device');
        },
        child: Icon(Icons.add),
        tooltip: 'Add Device',
      ),
    );
  }

  Widget _buildDeviceCard(DeviceModel device) {
    IconData iconData;
    Color cardColor;

    switch (device.type) {
      case 'switch':
        iconData = Icons.power_settings_new;
        cardColor = device.isOn ? Colors.green.shade100 : Colors.grey.shade200;
        break;
      case 'bulb':
        iconData = Icons.lightbulb;
        cardColor = device.isOn ? Colors.amber.shade100 : Colors.grey.shade200;
        break;
      case 'camera':
        iconData = Icons.camera_alt;
        cardColor = device.isOn ? Colors.blue.shade100 : Colors.grey.shade200;
        break;
      case 'smoke_detector':
        iconData = Icons.smoke_free;
        cardColor = device.isOn ? Colors.red.shade100 : Colors.grey.shade200;
        break;
      case 'blind':
        iconData = Icons.blinds;
        cardColor = device.isOn ? Colors.purple.shade100 : Colors.grey.shade200;
        break;
      default:
        iconData = Icons.devices;
        cardColor = device.isOn ? Colors.blue.shade100 : Colors.grey.shade200;
    }

    return Card(
      color: cardColor,
      elevation: 2,
      child: InkWell(
        onTap: () {
          Navigator.pushNamed(
            context,
            '/device_details',
            arguments: device,
          );
        },
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              iconData,
              size: 48,
              color: device.isOn ? Theme.of(context).primaryColor : Colors.grey,
            ),
            SizedBox(height: 8),
            Text(
              device.name,
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
              ),
              textAlign: TextAlign.center,
            ),
            SizedBox(height: 4),
            Text(
              device.isOn ? 'ON' : 'OFF',
              style: TextStyle(
                color: device.isOn ? Colors.green : Colors.grey,
              ),
            ),
            if (device.type == 'smoke_detector' && device.value != null)
              Padding(
                padding: const EdgeInsets.only(top: 4.0),
                child: Text(
                  'Level: ${device.value}',
                  style: TextStyle(
                    color: double.parse(device.value!) > 100 
                        ? Colors.red 
                        : Colors.black54,
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }
}

// Add device screen for connecting new devices
class AddDeviceScreen extends StatefulWidget {
  @override
  _AddDeviceScreenState createState() => _AddDeviceScreenState();
}

class _AddDeviceScreenState extends State<AddDeviceScreen> {
  final _formKey = GlobalKey<FormState>();
  final TextEditingController _deviceNameController = TextEditingController();
  final TextEditingController _deviceIdController = TextEditingController();
  String _selectedDeviceType = 'switch';
  bool _isScanning = false;
  bool _isConnecting = false;
  List<WiFiNetwork> _availableNetworks = [];
  WiFiNetwork? _selectedNetwork;
  final TextEditingController _wifiPasswordController = TextEditingController();
  String _statusMessage = '';

  // List of device types
  final List<Map<String, dynamic>> _deviceTypes = [
    {'type': 'switch', 'name': 'Smart Switch', 'icon': Icons.power_settings_new},
    {'type': 'bulb', 'name': 'Smart Bulb', 'icon': Icons.lightbulb},
    {'type': 'camera', 'name': 'Smart Camera', 'icon': Icons.camera_alt},
    {'type': 'smoke_detector', 'name': 'Smoke Detector', 'icon': Icons.smoke_free},
    {'type': 'blind', 'name': 'Window Blind', 'icon': Icons.blinds},
  ];

  @override
  void initState() {
    super.initState();
    _checkPermissions();
  }

  Future<void> _checkPermissions() async {
    // TODO: Implement permission checking for WiFi scanning
  }

  Future<void> _scanForDevices() async {
    setState(() {
      _isScanning = true;
      _statusMessage = 'Scanning for device hotspots...';
      _availableNetworks = [];
    });

    try {
      // Simulating network scanning
      // In a real app, use WiFiIoT package to scan for networks
      await Future.delayed(Duration(seconds: 2));
      
      setState(() {
        _availableNetworks = [
          WiFiNetwork(ssid: 'ESP_SWITCH_001', level: -50),
          WiFiNetwork(ssid: 'ESP_BULB_002', level: -55),
          WiFiNetwork(ssid: 'ESP_CAMERA_003', level: -60),
          WiFiNetwork(ssid: 'ESP_SMOKE_004', level: -65),
          WiFiNetwork(ssid: 'ESP_BLIND_005', level: -70),
        ];
      });
    } catch (e) {
      setState(() {
        _statusMessage = 'Error scanning: $e';
      });
    } finally {
      setState(() {
        _isScanning = false;
      });
    }
  }

  Future<void> _connectToDevice() async {
    if (_selectedNetwork == null) {
      setState(() {
        _statusMessage = 'Please select a device first';
      });
      return;
    }

    setState(() {
      _isConnecting = true;
      _statusMessage = 'Connecting to ${_selectedNetwork!.ssid}...';
    });

    try {
      // Simulate connection to device hotspot
      await Future.delayed(Duration(seconds: 2));
      
      // Extract device ID from SSID (in real app, this might come from the scan result)
      String deviceId = _selectedNetwork!.ssid.split('_').last;
      _deviceIdController.text = deviceId;
      
      // Guess device type from SSID
      String deviceType = _selectedNetwork!.ssid.split('_')[1].toLowerCase();
      setState(() {
        _selectedDeviceType = deviceType;
        _deviceNameController.text = '${deviceType.capitalize()} $deviceId';
      });
      
      setState(() {
        _statusMessage = 'Connected! Please configure the device.';
      });
    } catch (e) {
      setState(() {
        _statusMessage = 'Error connecting: $e';
      });
    } finally {
      setState(() {
        _isConnecting = false;
      });
    }
  }

  Future<void> _configureDevice() async {
    if (_formKey.currentState!.validate()) {
      setState(() {
        _statusMessage = 'Configuring device...';
      });

      try {
        // In a real app, we would send the configuration to the device via HTTP
        // For now, we simulate the process
        await Future.delayed(Duration(seconds: 2));
        
        // Then, register the device with the server
        final prefs = await SharedPreferences.getInstance();
        final token = prefs.getString('token');
        
        final response = await http.post(
          Uri.parse('https://well-scallop-cybergenii-075601d4.koyeb.app/api/devices'),
          headers: {
            'Authorization': 'Bearer $token',
            'Content-Type': 'application/json',
          },
          body: jsonEncode({
            'name': _deviceNameController.text,
            'id': _deviceIdController.text,
            'type': _selectedDeviceType,
            'wifiSSID': _selectedNetwork!.ssid,
            'hubConnected': true,
          }),
        );
        
        if (response.statusCode == 201) {
          setState(() {
            _statusMessage = 'Device added successfully!';
          });
          
          // Navigate back to dashboard after short delay
          Future.delayed(Duration(seconds: 1), () {
            Navigator.pop(context);
          });
        } else {
          setState(() {
            _statusMessage = 'Failed to register device with server';
          });
        }
      } catch (e) {
        setState(() {
          _statusMessage = 'Error configuring device: $e';
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Add New Device'),
      ),
      body: Stepper(
        currentStep: _selectedNetwork == null ? 0 : 1,
        onStepContinue: () {
          if (_selectedNetwork == null) {
            _connectToDevice();
          } else {
            _configureDevice();
          }
        },
        onStepCancel: () {
          if (_selectedNetwork != null) {
            setState(() {
              _selectedNetwork = null;
              _statusMessage = '';
            });
          } else {
            Navigator.pop(context);
          }
        },
        steps: [
          Step(
            title: Text('Scan for Devices'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Turn on your device and wait for it to create a hotspot.',
                  style: TextStyle(fontSize: 16),
                ),
                SizedBox(height: 16),
                ElevatedButton(
                  onPressed: _isScanning ? null : _scanForDevices,
                  child: _isScanning
                      ? CircularProgressIndicator(color: Colors.white)
                      : Text('Scan for Devices'),
                  style: ElevatedButton.styleFrom(
                    minimumSize: Size(double.infinity, 48),
                  ),
                ),
                SizedBox(height: 16),
                if (_availableNetworks.isNotEmpty) ...[
                  Text(
                    'Available Devices:',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  SizedBox(height: 8),
                  ListView.builder(
                    shrinkWrap: true,
                    physics: NeverScrollableScrollPhysics(),
                    itemCount: _availableNetworks.length,
                    itemBuilder: (context, index) {
                      final network = _availableNetworks[index];
                      return ListTile(
                        leading: Icon(Icons.wifi),
                        title: Text(network.ssid),
                        subtitle: Text('Signal: ${network.level} dBm'),
                        selected: _selectedNetwork?.ssid == network.ssid,
                        onTap: () {
                          setState(() {
                            _selectedNetwork = network;
                          });
                        },
                      );
                    },
                  ),
                ],
                if (_statusMessage.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.only(top: 16.0),
                    child: Text(
                      _statusMessage,
                      style: TextStyle(
                        color: _statusMessage.contains('Error')
                            ? Colors.red
                            : Colors.blue,
                      ),
                    ),
                  ),
              ],
            ),
            isActive: _selectedNetwork == null,
          ),
          Step(
            title: Text('Configure Device'),
            content: Form(
              key: _formKey,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Connected to: ${_selectedNetwork?.ssid}',
                    style: TextStyle(
                      fontSize: 16,
                      color: Colors.green,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  SizedBox(height: 16),
                  DropdownButtonFormField<String>(
                    decoration: InputDecoration(
                      labelText: 'Device Type',
                      border: OutlineInputBorder(),
                    ),
                    value: _selectedDeviceType,
                    items: _deviceTypes.map((deviceType) {
                      return DropdownMenuItem<String>(
                        value: deviceType['type'],
                        child: Row(
                          children: [
                            Icon(deviceType['icon']),
                            SizedBox(width: 10),
                            Text(deviceType['name']),
                          ],
                        ),
                      );
                    }).toList(),
                    onChanged: (value) {
                      setState(() {
                        _selectedDeviceType = value!;
                      });
                    },
                    validator: (value) {
                      if (value == null || value.isEmpty) {
                        return 'Please select device type';
                      }
                      return null;
                    },
                  ),
                  SizedBox(height: 16),
                  TextFormField(
                    controller: _deviceNameController,
                    decoration: InputDecoration(
                      labelText: 'Device Name',
                      border: OutlineInputBorder(),
                    ),
                    validator: (value) {
                      if (value == null || value.isEmpty) {
                        return 'Please enter device name';
                      }
                      return null;
                    },
                  ),
                  SizedBox(height: 16),
                  TextFormField(
                    controller: _deviceIdController,
                    decoration: InputDecoration(
                      labelText: 'Device ID',
                      border: OutlineInputBorder(),
                    ),
                    readOnly: true,
                  ),
                  SizedBox(height: 16),
                  Text(
                    'Connect device to your home WiFi:',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  SizedBox(height: 8),
                  TextFormField(
                    controller: _wifiPasswordController,
                    decoration: InputDecoration(
                      labelText: 'WiFi Password',
                      border: OutlineInputBorder(),
                    ),
                    obscureText: true,
                    validator: (value) {
                      if (value == null || value.isEmpty) {
                        return 'Please enter WiFi password';
                      }
                      return null;
                    },
                  ),
                  if (_statusMessage.isNotEmpty)
                    Padding(
                      padding: const EdgeInsets.only(top: 16.0),
                      child: Text(
                        _statusMessage,
                        style: TextStyle(
                          color: _statusMessage.contains('Error')
                              ? Colors.red
                              : _statusMessage.contains('success')
                                  ? Colors.green
                                  : Colors.blue,
                        ),
                      ),
                    ),
                ],
              ),
            ),
            isActive: _selectedNetwork != null,
          ),
        ],
      ),
    );
  }
}

// Device details screen for controlling a specific devi    ce
class DeviceDetailsScreen extends StatefulWidget {
  @override
  _DeviceDetailsScreenState createState() => _DeviceDetailsScreenState();
}

class _DeviceDetailsScreenState extends State<DeviceDetailsScreen> {
  bool _isLoading = true;
  DeviceModel? _device;
  String _errorMessage = '';

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _loadDeviceDetails();
  }

  Future<void> _loadDeviceDetails() async {
    final device = ModalRoute.of(context)!.settings.arguments as DeviceModel;
    setState(() {
      _device = device;
      _isLoading = false;
    });
  }

  Future<void> _toggleDevice() async {
    setState(() {
      _isLoading = true;
    });

    try {
      final prefs = await SharedPreferences.getInstance();
      final token = prefs.getString('token');

      final response = await http.post(
        Uri.parse('https://well-scallop-cybergenii-075601d4.koyeb.app/api/devices/${_device!.id}/toggle'),
        headers: {
          'Authorization': 'Bearer $token',
        },
      );

      if (response.statusCode == 200) {
        final updatedDevice = DeviceModel.fromJson(jsonDecode(response.body));
        setState(() {
          _device = updatedDevice;
        });
      } else {
        setState(() {
          _errorMessage = 'Failed to toggle device';
        });
      }
    } catch (e) {
      setState(() {
        _errorMessage = 'Network error: $e';
      });
    } finally {
      setState(() {
        _isLoading = false;
      });
    }
  }

  Widget _buildDeviceSpecificControls() {
    if (_device == null) return Container();

    switch (_device!.type) {
      case 'switch':
        return _buildSwitchControls();
      case 'bulb':
        return _buildBulbControls();
      case 'camera':
        return _buildCameraControls();
      case 'smoke_detector':
        return _buildSmokeDetectorControls();
      case 'blind':
        return _buildBlindControls();
      default:
        return Container(
          padding: EdgeInsets.all(16),
          child: Text('No specific controls for this device type'),
        );
    }
  }

  Widget _buildSwitchControls() {
    return Card(
      margin: EdgeInsets.all(16),
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Power Control',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
              ),
            ),
            SizedBox(height: 16),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'Switch Power',
                  style: TextStyle(fontSize: 16),
                ),
                Switch(
                  value: _device!.isOn,
                  onChanged: (value) {
                    _toggleDevice();
                  },
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildBulbControls() {
    return Card(
      margin: EdgeInsets.all(16),
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Light Control',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
              ),
            ),
            SizedBox(height: 16),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'Light Power',
                  style: TextStyle(fontSize: 16),
                ),
                Switch(
                  value: _device!.isOn,
                  onChanged: (value) {
                    _toggleDevice();
                  },
                ),
              ],
            ),
            // Additional controls for brightness would go here
          ],
        ),
      ),
    );
  }

  Widget _buildCameraControls() {
    return Column(
      children: [
        Card(
          margin: EdgeInsets.all(16),
          child: Padding(
            padding: EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Camera Control',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                SizedBox(height: 16),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      'Camera Power',
                      style: TextStyle(fontSize: 16),
                    ),
                    Switch(
                      value: _device!.isOn,
                      onChanged: (value) {
                        _toggleDevice();
                      },
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
        if (_device!.isOn)
          Card(
            margin: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                AspectRatio(
                  aspectRatio: 4/3,
                  child: Container(
                    color: Colors.black,
                    child: Center(
                      child: Text(
                        'Camera Feed',
                        style: TextStyle(color: Colors.white),
                      ),
                    ),
                  ),
                ),
                Padding(
                  padding: EdgeInsets.all(16),
                  child: Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      ElevatedButton.icon(
                        onPressed: () {},
                        icon: Icon(Icons.camera),
                        label: Text('Take Photo'),
                      ),
                      ElevatedButton.icon(
                        onPressed: () {},
                        icon: Icon(Icons.people),
                        label: Text('Manage Family'),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }

  Widget _buildSmokeDetectorControls() {
    final double smokeLevel = _device!.value != null ? double.parse(_device!.value!) : 0.0;
    final bool isAlarm = smokeLevel > 100;
    
    return Column(
      children: [
        Card(
          margin: EdgeInsets.all(16),
          child: Padding(
            padding: EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      'Smoke Detector Status',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    Container(
                      padding: EdgeInsets.symmetric(horizontal: 8, vertical: 4),decoration: BoxDecoration(
                        borderRadius: BorderRadius.circular(4),
                        color: isAlarm ? Colors.red : Colors.green,
                      ),

















            child: Text(
              isAlarm ? 'ALARM' : 'NORMAL',
              style: TextStyle(
                color: Colors.white,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
        ],
      ),
      SizedBox(height: 16),
      Text(
        'Current Smoke Level:',
        style: TextStyle(fontSize: 16),
      ),
      SizedBox(height: 8),
      LinearProgressIndicator(
        value: smokeLevel / 200,
        backgroundColor: Colors.grey.shade200,
        valueColor: AlwaysStoppedAnimation<Color>(
          isAlarm ? Colors.red : Colors.green,
        ),
      ),
      SizedBox(height: 8),
      Text(
        '${smokeLevel.toStringAsFixed(1)} ppm',
        style: TextStyle(
          fontSize: 18,
          fontWeight: FontWeight.bold,
          color: isAlarm ? Colors.red : Colors.green,
        ),
      ),
    ],
  ),
),
),
Card(
  margin: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
  child: Padding(
    padding: EdgeInsets.all(16),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Device Tests',
          style: TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
        SizedBox(height: 16),
        ElevatedButton.icon(
          onPressed: () {},
          icon: Icon(Icons.notification_important),
          label: Text('Test Alarm'),
          style: ElevatedButton.styleFrom(
            minimumSize: Size(double.infinity, 48),
          ),
        ),
        SizedBox(height: 8),
        OutlinedButton.icon(
          onPressed: () {},
          icon: Icon(Icons.battery_alert),
          label: Text('Check Battery'),
          style: OutlinedButton.styleFrom(
            minimumSize: Size(double.infinity, 48),
          ),
        ),
      ],
    ),
  ),
),
],
);
}

Widget _buildBlindControls() {
  return Column(
    children: [
      Card(
        margin: EdgeInsets.all(16),
        child: Padding(
          padding: EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Blind Control',
                style: TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.bold,
                ),
              ),
              SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Text(
                    'Blind Power',
                    style: TextStyle(fontSize: 16),
                  ),
                  Switch(
                    value: _device!.isOn,
                    onChanged: (value) {
                      _toggleDevice();
                    },
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
      if (_device!.isOn)
        Card(
          margin: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Padding(
            padding: EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Position Control',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                SizedBox(height: 16),
                Text(
                  'Current Position: ${_device!.value ?? '0'}%',
                  style: TextStyle(fontSize: 16),
                ),
                SizedBox(height: 8),
                Slider(
                  value: _device!.value != null
                      ? double.parse(_device!.value!)
                      : 0.0,
                  min: 0,
                  max: 100,
                  divisions: 10,
                  label: '${(_device!.value != null ? double.parse(_device!.value!) : 0).round()}%',
                  onChanged: (value) {
                    // Handle slider value changes
                  },
                  onChangeEnd: (value) {
                    // Send the final position to the server
                  },
                ),
                SizedBox(height: 16),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    ElevatedButton.icon(
                      onPressed: () {},
                      icon: Icon(Icons.arrow_upward),
                      label: Text('Open Fully'),
                    ),
                    ElevatedButton.icon(
                      onPressed: () {},
                      icon: Icon(Icons.arrow_downward),
                      label: Text('Close Fully'),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      Card(
        margin: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        child: Padding(
          padding: EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Calibration',
                style: TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.bold,
                ),
              ),
              SizedBox(height: 16),
              OutlinedButton.icon(
                onPressed: () {},
                icon: Icon(Icons.settings_applications),
                label: Text('Calibrate Motor'),
                style: OutlinedButton.styleFrom(
                  minimumSize: Size(double.infinity, 48),
                ),
              ),
            ],
          ),
        ),
      ),
    ],
  );
}

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(
      title: Text(_device?.name ?? 'Device Details'),
      actions: [
        IconButton(
          icon: Icon(Icons.delete),
          onPressed: () {
            showDialog(
              context: context,
              builder: (context) => AlertDialog(
                title: Text('Delete Device'),
                content: Text('Are you sure you want to delete this device?'),
                actions: [
                  TextButton(
                    onPressed: () {
                      Navigator.pop(context);
                    },
                    child: Text('Cancel'),
                  ),
                  TextButton(
                    onPressed: () {
                      // Handle device deletion
                      Navigator.pop(context);
                      Navigator.pop(context);
                    },
                    child: Text('Delete', style: TextStyle(color: Colors.red)),
                  ),
                ],
              ),
            );
          },
        ),
      ],
    ),
    body: _isLoading
        ? Center(child: CircularProgressIndicator())
        : _device == null
            ? Center(child: Text('Device not found'))
            : SingleChildScrollView(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Container(
                      width: double.infinity,
                      padding: EdgeInsets.all(16),
                      color: Theme.of(context).primaryColor.withOpacity(0.1),
                      child: Column(
                        children: [
                          Icon(
                            _getDeviceIcon(),
                            size: 64,
                            color: _device!.isOn
                                ? Theme.of(context).primaryColor
                                : Colors.grey,
                          ),
                          SizedBox(height: 8),
                          Text(
                            _device!.name,
                            style: TextStyle(
                              fontSize: 24,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          SizedBox(height: 4),
                          Text(
                            _device!.isOn ? 'Status: ON' : 'Status: OFF',
                            style: TextStyle(
                              fontSize: 16,
                              color: _device!.isOn ? Colors.green : Colors.grey,
                            ),
                          ),
                        ],
                      ),
                    ),
                    _buildDeviceSpecificControls(),
                    Card(
                      margin: EdgeInsets.all(16),
                      child: Padding(
                        padding: EdgeInsets.all(16),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              'Device Information',
                              style: TextStyle(
                                fontSize: 18,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            SizedBox(height: 16),
                            _buildInfoRow('Device ID', _device!.id),
                            _buildInfoRow('Device Type', _device!.type.capitalize()),
                            _buildInfoRow('Connected to Hub', 'Yes'),
                            _buildInfoRow('Last Updated', 'Just now'),
                          ],
                        ),
                      ),
                    ),
                  ],
                ),
              ),
  );
}

Widget _buildInfoRow(String label, String value) {
  return Padding(
    padding: const EdgeInsets.only(bottom: 8.0),
    child: Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(
          label,
          style: TextStyle(fontSize: 16, color: Colors.grey.shade700),
        ),
        Text(
          value,
          style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500),
        ),
      ],
    ),
  );
}

IconData _getDeviceIcon() {
  switch (_device!.type) {
    case 'switch':
      return Icons.power_settings_new;
    case 'bulb':
      return Icons.lightbulb;
    case 'camera':
      return Icons.camera_alt;
    case 'smoke_detector':
      return Icons.smoke_free;
    case 'blind':
      return Icons.blinds;
    default:
      return Icons.devices;
  }
}
}

// Camera setup screen
class CameraSetupScreen extends StatefulWidget {
@override
_CameraSetupScreenState createState() => _CameraSetupScreenState();
}

class _CameraSetupScreenState extends State<CameraSetupScreen> {
final List<FamilyMember> _familyMembers = [];
bool _isLoading = false;

Future<void> _addFamilyMember() async {
  // In a real app, this would use the camera to take a photo
  // For now, we'll just add a placeholder
  setState(() {
    _isLoading = true;
  });
  
  await Future.delayed(Duration(seconds: 1));
  
  setState(() {
    _familyMembers.add(
      FamilyMember(
        id: DateTime.now().millisecondsSinceEpoch.toString(),
        name: 'Family Member ${_familyMembers.length + 1}',
        photoUrl: null,
      ),
    );
    _isLoading = false;
  });
}

Future<void> _saveCameraSettings() async {
  setState(() {
    _isLoading = true;
  });
  
  await Future.delayed(Duration(seconds: 2));
  
  // In a real app, we would send the family members to the server
  
  setState(() {
    _isLoading = false;
  });
  
  Navigator.pop(context);
}

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(
      title: Text('Camera Setup'),
    ),
    body: Stack(
      children: [
        ListView(
          padding: EdgeInsets.all(16),
          children: [
            Card(
              child: Padding(
                padding: EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Family Members',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    SizedBox(height: 8),
                    Text(
                      'Add family members for facial recognition',
                      style: TextStyle(color: Colors.grey.shade700),
                    ),
                    SizedBox(height: 16),
                    if (_familyMembers.isEmpty)
                      Center(
                        child: Padding(
                          padding: EdgeInsets.all(32),
                          child: Text(
                            'No family members added yet',
                            style: TextStyle(color: Colors.grey),
                          ),
                        ),
                      )
                    else
                      ListView.builder(
                        shrinkWrap: true,
                        physics: NeverScrollableScrollPhysics(),
                        itemCount: _familyMembers.length,
                        itemBuilder: (context, index) {
                          final member = _familyMembers[index];
                          return ListTile(
                            leading: CircleAvatar(
                              backgroundColor: Colors.grey.shade200,
                              child: Icon(Icons.person),
                            ),
                            title: Text(member.name),
                            trailing: IconButton(
                              icon: Icon(Icons.delete),
                              onPressed: () {
                                setState(() {
                                  _familyMembers.removeAt(index);
                                });
                              },
                            ),
                          );
                        },
                      ),
                    SizedBox(height: 16),
                    ElevatedButton.icon(
                      onPressed: _addFamilyMember,
                      icon: Icon(Icons.person_add),
                      label: Text('Add Family Member'),
                      style: ElevatedButton.styleFrom(
                        minimumSize: Size(double.infinity, 48),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            Card(
              child: Padding(
                padding: EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Camera Settings',
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    SizedBox(height: 16),
                    SwitchListTile(
                      title: Text('Motion Detection'),
                      subtitle: Text('Send notifications on movement'),
                      value: true,
                      onChanged: (value) {},
                    ),
                    SwitchListTile(
                      title: Text('Facial Recognition'),
                      subtitle: Text('Identify family members'),
                      value: true,
                      onChanged: (value) {},
                    ),
                    SwitchListTile(
                      title: Text('Record Video'),
                      subtitle: Text('Save video clips on motion'),
                      value: false,
                      onChanged: (value) {},
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 32),
            ElevatedButton(
              onPressed: _saveCameraSettings,
              child: Text('Save Settings'),
              style: ElevatedButton.styleFrom(
                minimumSize: Size(double.infinity, 48),
              ),
            ),
          ],
        ),
        if (_isLoading)
          Container(
            color: Colors.black.withOpacity(0.3),
            child: Center(
              child: CircularProgressIndicator(),
            ),
          ),
      ],
    ),
  );
}
}

// Settings screen
class SettingsScreen extends StatefulWidget {
@override
_SettingsScreenState createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
String _username = '';
bool _isDarkMode = false;
bool _notificationsEnabled = true;
bool _isLoading = true;

@override
void initState() {
  super.initState();
  _loadSettings();
}

Future<void> _loadSettings() async {
  // Simulate loading settings
  await Future.delayed(Duration(milliseconds: 500));
  
  final prefs = await SharedPreferences.getInstance();
  setState(() {
    _username = prefs.getString('username') ?? '';
    _isDarkMode = prefs.getBool('darkMode') ?? false;
    _notificationsEnabled = prefs.getBool('notifications') ?? true;
    _isLoading = false;
  });
}

Future<void> _saveSettings() async {
  setState(() {
    _isLoading = true;
  });
  
  final prefs = await SharedPreferences.getInstance();
  await prefs.setBool('darkMode', _isDarkMode);
  await prefs.setBool('notifications', _notificationsEnabled);
  
  setState(() {
    _isLoading = false;
  });
  
  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(content: Text('Settings saved')),
  );
}

Future<void> _logout() async {
  setState(() {
    _isLoading = true;
  });
  
  final prefs = await SharedPreferences.getInstance();
  await prefs.remove('token');
  await prefs.remove('username');
  
  setState(() {
    _isLoading = false;
  });
  
  Navigator.pushReplacementNamed(context, '/');
}

@override
Widget build(BuildContext context) {
  return Scaffold(
    appBar: AppBar(
      title: Text('Settings'),
    ),
    body: _isLoading
        ? Center(child: CircularProgressIndicator())
        : ListView(
            padding: EdgeInsets.all(16),
            children: [
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Account',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 16),
                      ListTile(
                        leading: CircleAvatar(
                          backgroundColor: Theme.of(context).primaryColor,
                          child: Text(
                            _username.isNotEmpty
                                ? _username[0].toUpperCase()
                                : 'U',
                            style: TextStyle(color: Colors.white),
                          ),
                        ),
                        title: Text(_username),
                        subtitle: Text('Logged in'),
                      ),
                      Divider(),
                      ListTile(
                        leading: Icon(Icons.security),
                        title: Text('Change Password'),
                        trailing: Icon(Icons.arrow_forward_ios, size: 16),
                        onTap: () {
                          // Navigate to change password screen
                        },
                      ),
                    ],
                  ),
                ),
              ),
              SizedBox(height: 16),
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Preferences',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 16),
                      SwitchListTile(
                        title: Text('Dark Mode'),
                        subtitle: Text('Use dark theme'),
                        value: _isDarkMode,
                        onChanged: (value) {
                          setState(() {
                            _isDarkMode = value;
                          });
                        },
                      ),
                      SwitchListTile(
                        title: Text('Notifications'),
                        subtitle: Text('Enable push notifications'),
                        value: _notificationsEnabled,
                        onChanged: (value) {
                          setState(() {
                            _notificationsEnabled = value;
                          });
                        },
                      ),
                    ],
                  ),
                ),
              ),
              SizedBox(height: 16),
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Hub',
                        style: TextStyle(
                          fontSize: 18,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 16),
                      ListTile(
                        leading: Icon(Icons.router),
                        title: Text('Hub Settings'),
                        trailing: Icon(Icons.arrow_forward_ios, size: 16),
                        onTap: () {
                          // Navigate to hub settings screen
                        },
                      ),
                      ListTile(
                        leading: Icon(Icons.wifi),
                        title: Text('Network Configuration'),
                        trailing: Icon(Icons.arrow_forward_ios, size: 16),
                        onTap: () {
                          // Navigate to network configuration screen
                        },
                      ),
                    ],
                  ),
                ),
              ),
              SizedBox(height: 32),
              ElevatedButton(
                onPressed: _saveSettings,
                child: Text('Save Settings'),
                style: ElevatedButton.styleFrom(
                  minimumSize: Size(double.infinity, 48),
                ),
              ),
              SizedBox(height: 16),
              OutlinedButton(
                onPressed: () {
                  showDialog(
                    context: context,
                    builder: (context) => AlertDialog(
                      title: Text('Log Out'),
                      content: Text('Are you sure you want to log out?'),
                      actions: [
                        TextButton(
                          onPressed: () {
                            Navigator.pop(context);
                          },
                          child: Text('Cancel'),
                        ),
                        TextButton(
                          onPressed: () {
                            Navigator.pop(context);
                            _logout();
                          },
                          child: Text('Log Out', style: TextStyle(color: Colors.red)),
                        ),
                      ],
                    ),
                  );
                },
                child: Text('Log Out'),
                style: OutlinedButton.styleFrom(
                  minimumSize: Size(double.infinity, 48),
                ),
              ),
            ],
          ),
  );
}
}

// Model classes
class DeviceModel {
final String id;
final String name;
final String type;
final bool isOn;
final String? value;

DeviceModel({
  required this.id,
  required this.name,
  required this.type,
  required this.isOn,
  this.value,
});

factory DeviceModel.fromJson(Map<String, dynamic> json) {
  return DeviceModel(
    id: json['id'],
    name: json['name'],
    type: json['type'],
    isOn: json['isOn'] ?? false,
    value: json['value']?.toString(),
  );
}
}

class FamilyMember {
final String id;
final String name;
final String? photoUrl;

FamilyMember({
  required this.id,
  required this.name,
  this.photoUrl,
});
}

class WiFiNetwork {
final String ssid;
final int level;

WiFiNetwork({
  required this.ssid,
  required this.level,
});
}

// Extension for string capitalization
extension StringExtension on String {
String capitalize() {
  return "${this[0].toUpperCase()}${this.substring(1)}";
}
}
