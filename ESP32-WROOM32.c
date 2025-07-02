// ESP32-WROOM-32 - Web Server for Sensor Data Display

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi credentials - to be filled in
const char* ssid = "Edward's 12 mini";     // Fill in your WiFi SSID
const char* password = "imiplacsarmalele"; // Fill in your WiFi password

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;  // GMT+2 (Romania timezone)
const int daylightOffset_sec = 3600;  // Daylight saving time offset

// Serial communication settings
#define BAUD_RATE 9600
#define SERIAL_SIZE_RX 1024

// Timing for sending time updates to PIC
unsigned long lastTimeUpdate = 0;
const unsigned long timeUpdateInterval = 10000; // Send time every 10 seconds
bool initialTimeSent = false; // Flag to track if initial time was sent

// Web server
AsyncWebServer server(80);

// Sensor data storage
struct SensorData {
  float lm35_temp = 0.0;
  float hih_humid = 0.0;
  float light = 0.0;
  float sht_temp = 0.0;
  float sht_humid = 0.0;
  bool lm35_valid = false;
  bool hih_valid = false;
  bool light_valid = false;
  bool sht_temp_valid = false;
  bool sht_humid_valid = false;
  unsigned long last_update = 0;
} sensorData;

// Buffer for incoming serial data
String serialBuffer = "";
bool isDataComplete = false;

// Function declarations
void setupWebServer();
void parseSerialData(String dataString);
void parseSensorToken(String token);
void sendTimeDataToPIC();
String getSensorDataJson();
String getCurrentTimeJson();
void parseJsonData(String jsonString);

// Embedded HTML content
const char* htmlContent = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PIC16F887 Sensor Hub</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        /* Modern CSS Reset */
        *, *::before, *::after {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        /* Theme variables */
        :root {
            --bg-primary: #f5f5f7;
            --bg-secondary: rgba(255, 255, 255, 0.8);
            --bg-card: #ffffff;
            --text-primary: #1d1d1f;
            --text-secondary: #86868b;
            --accent-color: #007aff;
            --success-color: #30d158;
            --error-color: #ff3b30;
            --border-color: rgba(0, 0, 0, 0.04);
            --shadow-color: rgba(0, 0, 0, 0.08);
        }

        [data-theme="dark"] {
            --bg-primary: #000000;
            --bg-secondary: rgba(28, 28, 30, 0.8);
            --bg-card: rgba(44, 44, 46, 0.8);
            --text-primary: #f5f5f7;
            --text-secondary: #86868b;
            --accent-color: #007aff;
            --success-color: #30d158;
            --error-color: #ff3b30;
            --border-color: rgba(255, 255, 255, 0.1);
            --shadow-color: rgba(0, 0, 0, 0.3);
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Display', 'Helvetica Neue', Arial, sans-serif;
            background: var(--bg-primary);
            min-height: 100vh;
            color: var(--text-primary);
            line-height: 1.47059;
            font-weight: 400;
            position: relative;
            overflow-x: hidden;
        }

        /* GTA 6 Inspired Background Blobs */
        .background-blobs {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
            z-index: -1;
            overflow: hidden;
        }

        .blob {
            position: absolute;
            border-radius: 50%;
            filter: blur(40px);
            opacity: 0.3;
            animation-timing-function: cubic-bezier(0.25, 0.46, 0.45, 0.94);
        }

        .blob-1 {
            width: 400px;
            height: 400px;
            background: radial-gradient(circle, #ff0080, #ff4da6);
            top: 10%;
            left: -20%;
            animation: drift1 25s infinite linear;
        }

        .blob-2 {
            width: 350px;
            height: 350px;
            background: radial-gradient(circle, #00d4ff, #0099cc);
            top: 60%;
            left: -15%;
            animation: drift2 30s infinite linear;
            animation-delay: -10s;
        }

        .blob-3 {
            width: 500px;
            height: 500px;
            background: radial-gradient(circle, #8a2be2, #6a1b9a);
            top: 30%;
            left: -25%;
            animation: drift3 35s infinite linear;
            animation-delay: -20s;
        }

        @keyframes drift1 {
            0% { transform: translate(-200px, 0px) rotate(0deg); }
            25% { transform: translate(400px, -100px) rotate(90deg); }
            50% { transform: translate(800px, 50px) rotate(180deg); }
            75% { transform: translate(1200px, -50px) rotate(270deg); }
            100% { transform: translate(1600px, 100px) rotate(360deg); }
        }

        @keyframes drift2 {
            0% { transform: translate(-200px, 0px) rotate(0deg) scale(1); }
            20% { transform: translate(300px, 150px) rotate(72deg) scale(1.2); }
            40% { transform: translate(600px, -100px) rotate(144deg) scale(0.8); }
            60% { transform: translate(900px, 200px) rotate(216deg) scale(1.1); }
            80% { transform: translate(1200px, -50px) rotate(288deg) scale(0.9); }
            100% { transform: translate(1500px, 100px) rotate(360deg) scale(1); }
        }

        @keyframes drift3 {
            0% { transform: translate(-300px, 0px) rotate(0deg) scale(1); }
            15% { transform: translate(200px, 200px) rotate(54deg) scale(0.7); }
            30% { transform: translate(500px, -150px) rotate(108deg) scale(1.3); }
            45% { transform: translate(750px, 100px) rotate(162deg) scale(0.9); }
            60% { transform: translate(1000px, -100px) rotate(216deg) scale(1.1); }
            75% { transform: translate(1250px, 150px) rotate(270deg) scale(0.8); }
            90% { transform: translate(1450px, -75px) rotate(324deg) scale(1.2); }
            100% { transform: translate(1700px, 50px) rotate(360deg) scale(1); }
        }

        [data-theme="dark"] .blob {
            opacity: 0.15;
            filter: blur(60px);
        }

        .container {
            max-width: 1440px;
            margin: 0 auto;
            padding: 24px;
            position: relative;
            z-index: 1;
        }

        /* Header Styles */
        .dashboard-header {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 18px;
            padding: 32px 40px;
            margin-bottom: 32px;
            box-shadow: 0 4px 16px 0 var(--shadow-color);
            border: 0.5px solid var(--border-color);
        }

        .header-content {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 24px;
        }

        .header-left h1 {
            font-size: 40px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 8px;
            letter-spacing: -0.022em;
        }

        .header-left h1 i {
            color: var(--accent-color);
            margin-right: 16px;
        }

        .header-left p {
            font-size: 19px;
            color: var(--text-secondary);
            font-weight: 400;
            letter-spacing: -0.022em;
        }

        .header-right {
            display: flex;
            flex-direction: column;
            align-items: flex-end;
            gap: 12px;
        }

        .theme-toggle {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 12px;
            border-radius: 20px;
            background: var(--bg-secondary);
            border: 0.5px solid var(--border-color);
            cursor: pointer;
            transition: all 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            font-size: 14px;
            color: var(--text-secondary);
            order: -1;
        }

        .theme-toggle:hover {
            background: rgba(0, 122, 255, 0.1);
            color: var(--accent-color);
        }

        .connection-indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 16px;
            border-radius: 20px;
            background: rgba(142, 142, 147, 0.12);
            backdrop-filter: blur(10px);
        }

        .connection-status.connected {
            color: var(--success-color);
            font-weight: 500;
            font-size: 15px;
        }

        .connection-status.disconnected {
            color: var(--error-color);
            font-weight: 500;
            font-size: 15px;
        }

        .last-update {
            display: flex;
            align-items: center;
            gap: 6px;
            color: var(--text-secondary);
            font-size: 15px;
            font-weight: 400;
        }

        /* Tab Navigation */
        .tab-navigation {
            display: flex;
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 12px;
            padding: 4px;
            margin-bottom: 32px;
            box-shadow: 0 2px 8px 0 var(--shadow-color);
            border: 0.5px solid var(--border-color);
            gap: 0;
        }

        .tab-btn {
            flex: 1;
            padding: 12px 20px;
            border: none;
            background: transparent;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            font-weight: 500;
            font-size: 15px;
            color: var(--text-secondary);
            letter-spacing: -0.022em;
        }

        .tab-btn:hover {
            background: rgba(0, 122, 255, 0.04);
            color: var(--accent-color);
        }

        .tab-btn.active {
            background: var(--accent-color);
            color: white;
            box-shadow: 0 1px 3px 0 rgba(0, 122, 255, 0.3);
        }

        .tab-btn i {
            margin-right: 6px;
            font-size: 14px;
        }

        /* Tab Content */
        .tab-content {
            display: none;
        }

        .tab-content.active {
            display: block;
            animation: fadeIn 0.5s ease;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* Stats Overview */
        .stats-overview {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            margin-bottom: 32px;
        }

        .stat-card {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 16px;
            padding: 24px;
            display: flex;
            align-items: center;
            gap: 20px;
            box-shadow: 0 2px 8px 0 var(--shadow-color);
            transition: transform 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            border: 0.5px solid var(--border-color);
        }

        .stat-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 16px 0 var(--shadow-color);
        }

        .stat-card i {
            font-size: 32px;
            padding: 16px;
            border-radius: 12px;
            background: var(--accent-color);
            color: white;
        }

        .stat-content h3 {
            font-size: 15px;
            color: var(--text-secondary);
            margin-bottom: 4px;
            font-weight: 500;
            letter-spacing: -0.022em;
        }

        .stat-value {
            font-size: 28px;
            font-weight: 600;
            color: var(--text-primary);
            letter-spacing: -0.022em;
        }

        /* Sensor Sections */
        .sensor-sections {
            display: flex;
            flex-direction: column;
            gap: 40px;
        }

        .sensor-section {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 18px;
            padding: 32px;
            box-shadow: 0 4px 16px 0 var(--shadow-color);
            border: 0.5px solid var(--border-color);
        }

        .sensor-section h2 {
            font-size: 28px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 24px;
            display: flex;
            align-items: center;
            gap: 12px;
            letter-spacing: -0.022em;
        }

        .sensor-section h2 i {
            color: var(--accent-color);
            font-size: 24px;
        }

        .sensor-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 20px;
        }

        /* Sensor Cards */
        .sensor-card {
            background: var(--bg-card);
            border-radius: 12px;
            padding: 24px;
            transition: all 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            border: 0.5px solid var(--border-color);
            position: relative;
            overflow: hidden;
            box-shadow: 0 1px 3px 0 var(--shadow-color);
        }

        .sensor-card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 2px;
            background: var(--accent-color);
            transform: scaleX(0);
            transition: transform 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
        }

        .sensor-card:hover::before {
            transform: scaleX(1);
        }

        .sensor-card:hover {
            transform: translateY(-4px);
            box-shadow: 0 8px 24px 0 rgba(0, 0, 0, 0.12);
        }

        .sensor-card.error {
            border-color: var(--error-color);
            background: rgba(255, 59, 48, 0.05);
        }

        .sensor-header {
            display: flex;
            align-items: center;
            gap: 12px;
            margin-bottom: 16px;
        }

        .sensor-header i {
            font-size: 20px;
            color: var(--accent-color);
            padding: 8px;
            background: rgba(0, 122, 255, 0.1);
            border-radius: 8px;
        }

        .sensor-header h3 {
            font-size: 17px;
            font-weight: 500;
            color: var(--text-primary);
            letter-spacing: -0.022em;
        }

        .sensor-value {
            font-size: 36px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 12px;
            text-align: center;
            letter-spacing: -0.022em;
        }

        .sensor-status {
            text-align: center;
            padding: 6px 12px;
            border-radius: 16px;
            font-size: 13px;
            font-weight: 500;
            text-transform: uppercase;
            letter-spacing: 0.06em;
        }

        .status-ok {
            background: rgba(48, 209, 88, 0.1);
            color: var(--success-color);
        }

        .status-error {
            background: rgba(255, 59, 48, 0.1);
            color: var(--error-color);
        }

        /* Light Visualization */
        .light-card {
            grid-column: span 2;
        }

        .light-visualization {
            margin: 16px 0;
            display: flex;
            align-items: center;
            gap: 12px;
        }

        .light-bar {
            flex: 1;
            height: 8px;
            background: rgba(142, 142, 147, 0.16);
            border-radius: 4px;
            overflow: hidden;
        }

        .light-indicator {
            height: 100%;
            background: linear-gradient(90deg, #ff9500, #ffcc02);
            border-radius: 4px;
            width: 0%;
            transition: width 0.3s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            position: relative;
        }

        .light-indicator::after {
            content: '';
            position: absolute;
            top: 0;
            right: 0;
            bottom: 0;
            width: 12px;
            background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.4));
            border-radius: 0 4px 4px 0;
        }

        #light-percentage {
            font-weight: 500;
            color: var(--text-primary);
            min-width: 40px;
            font-size: 15px;
        }

        /* Charts Section */
        .charts-section {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 24px;
            padding: 0;
        }

        .chart-container {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 16px;
            padding: 24px;
            box-shadow: 0 2px 8px 0 var(--shadow-color);
            border: 0.5px solid var(--border-color);
        }

        .chart-container h3 {
            font-size: 22px;
            font-weight: 600;
            color: var(--text-primary);
            display: flex; /* Added to align icon and text */
            align-items: center; /* Moved and corrected */
            gap: 8px; /* Moved and corrected */
            letter-spacing: -0.022em; /* Moved and corrected */
        } /* Closing brace added */

        .chart-container h3 i {
            color: var(--accent-color);
            font-size: 18px;
        }

        /* Gauges Section */
        .gauges-section {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 24px;
            padding: 0;
        }

        .gauge-container {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 16px;
            padding: 24px;
            text-align: center;
            box-shadow: 0 2px 8px 0 var(--shadow-color);
            transition: transform 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            border: 0.5px solid var(--border-color);
        }

        .gauge-container:hover {
            transform: translateY(-2px);
        }

        .gauge-container h3 {
            font-size: 19px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 16px;
            letter-spacing: -0.022em;
        }

        /* Footer */
        .dashboard-footer {
            background: var(--bg-secondary);
            backdrop-filter: saturate(180%) blur(20px);
            border-radius: 16px;
            padding: 16px;
            margin-top: 32px;
            text-align: center;
            box-shadow: 0 2px 8px 0 var(--shadow-color);
            border: 0.5px solid var(--border-color);
        }

        .footer-content p {
            color: var(--text-secondary);
            font-size: 13px;
            font-weight: 400;
        }

        /* Responsive Design */
        @media (max-width: 768px) {
            .container { padding: 16px; }
            .header-content { flex-direction: column; text-align: center; }
            .header-right { align-items: center; }
            .tab-navigation { flex-direction: column; gap: 2px; }
            .charts-section { grid-template-columns: 1fr; }
            .sensor-grid { grid-template-columns: 1fr; }
            .light-card { grid-column: span 1; }
            .stats-overview { grid-template-columns: 1fr; }
            .header-left h1 { font-size: 32px; }
            .sensor-value { font-size: 28px; }
            .blob-1 { width: 250px; height: 250px; }
            .blob-2 { width: 200px; height: 200px; }
            .blob-3 { width: 300px; height: 300px; }
            .blob { filter: blur(30px); opacity: 0.2; }
        }

        @media (max-width: 480px) {
            .sensor-card { padding: 20px; }
            .sensor-section { padding: 24px; }
            .dashboard-header { padding: 24px; }
        }

        @media (prefers-reduced-motion: reduce) {
            .blob { animation-duration: 60s; filter: blur(60px); opacity: 0.1; }
        }

        * {
            transition: color 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94), 
                       background-color 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94), 
                       border-color 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94), 
                       transform 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94), 
                       box-shadow 0.2s cubic-bezier(0.25, 0.46, 0.45, 0.94);
        }
    </style>
    <script>
        // Auto-refresh data every 5 seconds
        const refreshInterval = 5000;
        let connectionStatus = 'disconnected';
        let temperatureChart, humidityChart;
        let tempHistory = [];
        let humidityHistory = [];
        let timeLabels = [];
        const maxDataPoints = 20;
        
        // Theme management
        function initTheme() {
            const savedTheme = localStorage.getItem('theme') || 'light';
            document.documentElement.setAttribute('data-theme', savedTheme);
            updateThemeToggle(savedTheme);
        }
        
        function toggleTheme() {
            const currentTheme = document.documentElement.getAttribute('data-theme');
            const newTheme = currentTheme === 'dark' ? 'light' : 'dark';
            
            document.documentElement.setAttribute('data-theme', newTheme);
            localStorage.setItem('theme', newTheme);
            updateThemeToggle(newTheme);
        }
        
        function updateThemeToggle(theme) {
            const themeIcon = document.getElementById('theme-icon');
            const themeText = document.getElementById('theme-text');
            
            if (theme === 'dark') {
                themeIcon.className = 'fas fa-sun';
                themeText.textContent = 'Light';
            } else {
                themeIcon.className = 'fas fa-moon';
                themeText.textContent = 'Dark';
            }
        }
        
        function initCharts() {
            // Temperature chart
            const tempCtx = document.getElementById('temperatureChart').getContext('2d');
            temperatureChart = new Chart(tempCtx, {
                type: 'line',
                data: {
                    labels: timeLabels,
                    datasets: [
                        {
                            label: 'LM35 Temperature',
                            data: [],
                            borderColor: '#ff6b6b',
                            backgroundColor: 'rgba(255, 107, 107, 0.1)',
                            tension: 0.4
                        },
                        {
                            label: 'SHT21 Temperature',
                            data: [],
                            borderColor: '#4ecdc4',
                            backgroundColor: 'rgba(78, 205, 196, 0.1)',
                            tension: 0.4
                        }
                    ]
                },
                options: {
                    responsive: true,
                    plugins: {
                        legend: { position: 'top' }
                    },
                    scales: {
                        y: { beginAtZero: false, title: { display: true, text: 'Temperature (°C)' } },
                        x: { title: { display: true, text: 'Time' } }
                    }
                }
            });
            
            // Humidity chart
            const humidCtx = document.getElementById('humidityChart').getContext('2d');
            humidityChart = new Chart(humidCtx, {
                type: 'line',
                data: {
                    labels: timeLabels,
                    datasets: [
                        {
                            label: 'HIH-5030 Humidity',
                            data: [],
                            borderColor: '#45b7d1',
                            backgroundColor: 'rgba(69, 183, 209, 0.1)',
                            tension: 0.4
                        },
                        {
                            label: 'SHT21 Humidity',
                            data: [],
                            borderColor: '#96ceb4',
                            backgroundColor: 'rgba(150, 206, 180, 0.1)',
                            tension: 0.4
                        }
                    ]
                },
                options: {
                    responsive: true,
                    plugins: {
                        legend: { position: 'top' }
                    },
                    scales: {
                        y: { beginAtZero: true, max: 100, title: { display: true, text: 'Humidity (%)' } },
                        x: { title: { display: true, text: 'Time' } }
                    }
                }
            });
        }
        
        function updateCharts(data) {
            const currentTime = new Date().toLocaleTimeString();
            
            // Update time labels
            timeLabels.push(currentTime);
            if (timeLabels.length > maxDataPoints) {
                timeLabels.shift();
            }
            
            // Update temperature data
            if (data.lm35_valid) {
                temperatureChart.data.datasets[0].data.push(data.lm35_temp);
            } else {
                temperatureChart.data.datasets[0].data.push(null);
            }
            
            if (data.sht_temp_valid) {
                temperatureChart.data.datasets[1].data.push(data.sht_temp);
            } else {
                temperatureChart.data.datasets[1].data.push(null);
            }
            
            // Update humidity data
            if (data.hih_valid) {
                humidityChart.data.datasets[0].data.push(data.hih_humid);
            } else {
                humidityChart.data.datasets[0].data.push(null);
            }
            
            if (data.sht_humid_valid) {
                humidityChart.data.datasets[1].data.push(data.sht_humid);
            } else {
                humidityChart.data.datasets[1].data.push(null);
            }
            
            // Remove old data points
            if (temperatureChart.data.datasets[0].data.length > maxDataPoints) {
                temperatureChart.data.datasets[0].data.shift();
                temperatureChart.data.datasets[1].data.shift();
                humidityChart.data.datasets[0].data.shift();
                humidityChart.data.datasets[1].data.shift();
            }
            
            // Update charts
            temperatureChart.data.labels = timeLabels;
            humidityChart.data.labels = timeLabels;
            temperatureChart.update('none');
            humidityChart.update('none');
        }
        
        function updateConnectionStatus(status) {
            connectionStatus = status;
            const statusElement = document.getElementById('connection-status');
            const statusIcon = document.getElementById('status-icon');
            
            if (status === 'connected') {
                statusElement.textContent = 'Connected';
                statusElement.className = 'connection-status connected';
                statusIcon.className = 'fas fa-wifi';
            } else {
                statusElement.textContent = 'Disconnected';
                statusElement.className = 'connection-status disconnected';
                statusIcon.className = 'fas fa-wifi-slash';
            }
        }
        
        function createGaugeChart(canvasId, value, max, unit, color) {
            const canvas = document.getElementById(canvasId);
            const ctx = canvas.getContext('2d');
            const centerX = canvas.width / 2;
            const centerY = canvas.height / 2;
            const radius = 80;
            
            // Clear canvas
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Background arc
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, 0.75 * Math.PI, 0.25 * Math.PI);
            ctx.strokeStyle = '#e0e0e0';
            ctx.lineWidth = 20;
            ctx.stroke();
            
            // Value arc
            const angle = 0.75 * Math.PI + (value / max) * 1.5 * Math.PI;
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, 0.75 * Math.PI, angle);
            ctx.strokeStyle = color;
            ctx.lineWidth = 20;
            ctx.stroke();
            
            // Value text
            ctx.fillStyle = '#333';
            ctx.font = 'bold 24px Arial';
            ctx.textAlign = 'center';
            ctx.fillText(value.toFixed(1) + unit, centerX, centerY + 10);
        }
        
        function updateSensorData() {
            fetch('/sensorData')
                .then(response => {
                    if (!response.ok) throw new Error('Network error');
                    return response.json();
                })
                .then(data => {
                    updateConnectionStatus('connected');
                    
                    // Update analog sensor data
                    updateSensorCard('lm35', data.lm35_valid, data.lm35_temp, '°C', 1);
                    updateSensorCard('hih', data.hih_valid, data.hih_humid, '%', 0);
                    updateSensorCard('ldr', data.light_valid, data.light, '%', 0);
                    
                    // Update digital sensor data
                    updateSensorCard('sht-temp', data.sht_temp_valid, data.sht_temp, '°C', 1);
                    updateSensorCard('sht-humid', data.sht_humid_valid, data.sht_humid, '%', 0);
                    
                    // Update light visualization
                    if (data.light_valid) {
                        const lightLevel = Math.min(100, Math.max(0, data.light));
                        document.getElementById('light-indicator').style.width = lightLevel + '%';
                        document.getElementById('light-percentage').textContent = lightLevel.toFixed(0) + '%';
                        
                        // Update light gauge
                        createGaugeChart('lightGauge', lightLevel, 100, '%', '#ffd93d');
                    }
                    
                    // Update temperature gauges
                    if (data.lm35_valid) {
                        createGaugeChart('lm35Gauge', data.lm35_temp, 50, '°C', '#ff6b6b');
                    }
                    if (data.sht_temp_valid) {
                        createGaugeChart('shtTempGauge', data.sht_temp, 50, '°C', '#4ecdc4');
                    }
                    
                    // Update charts
                    updateCharts(data);
                    
                    // Update statistics
                    updateStatistics(data);
                    
                    // Update last update time
                    const lastUpdateTime = new Date().toLocaleTimeString();
                    document.getElementById('last-update').textContent = lastUpdateTime;
                })
                .catch(error => {
                    console.error('Error fetching sensor data:', error);
                    updateConnectionStatus('disconnected');
                });
        }
        
        function updateSensorCard(sensorId, isValid, value, unit, decimals) {
            const valueElement = document.getElementById(sensorId + '-value');
            const statusElement = document.getElementById(sensorId + '-status');
            const cardElement = document.querySelector(`[data-sensor="${sensorId}"]`);
            
            if (isValid) {
                valueElement.textContent = value.toFixed(decimals) + unit;
                statusElement.textContent = 'Online';
                statusElement.className = 'sensor-status status-ok';
                cardElement.classList.remove('error');
            } else {
                valueElement.textContent = 'N/A';
                statusElement.textContent = 'Error';
                statusElement.className = 'sensor-status status-error';
                cardElement.classList.add('error');
            }
        }
        
        function updateStatistics(data) {
            // Calculate averages (simplified - in real app would use historical data)
            let tempSum = 0, tempCount = 0;
            let humidSum = 0, humidCount = 0;
            
            if (data.lm35_valid) { tempSum += data.lm35_temp; tempCount++; }
            if (data.sht_temp_valid) { tempSum += data.sht_temp; tempCount++; }
            if (data.hih_valid) { humidSum += data.hih_humid; humidCount++; }
            if (data.sht_humid_valid) { humidSum += data.sht_humid; humidCount++; }
            
            document.getElementById('avg-temp').textContent = tempCount > 0 ? (tempSum / tempCount).toFixed(1) + '°C' : 'N/A';
            document.getElementById('avg-humid').textContent = humidCount > 0 ? (humidSum / humidCount).toFixed(0) + '%' : 'N/A';
            document.getElementById('light-level').textContent = data.light_valid ? data.light.toFixed(0) + '%' : 'N/A';
        }
        
        // Initial setup when page loads
        window.onload = function() {
            initTheme();
            initCharts();
            updateSensorData();
            setInterval(updateSensorData, refreshInterval);
            
            // Add click handlers for tabs
            document.querySelectorAll('.tab-btn').forEach(btn => {
                btn.addEventListener('click', () => {
                    const target = btn.dataset.tab;
                    
                    // Update active tab
                    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
                    btn.classList.add('active');
                    
                    // Update active content
                    document.querySelectorAll('.tab-content').forEach(content => {
                        content.classList.remove('active');
                    });
                    document.getElementById(target).classList.add('active');
                });
            });
        };
    </script>
</head>
<body>
    <!-- GTA 6 Inspired Background Blobs -->
    <div class="background-blobs">
        <div class="blob blob-1"></div>
        <div class="blob blob-2"></div>
        <div class="blob blob-3"></div>
    </div>
    
    <div class="container">
        <header class="dashboard-header">
            <div class="header-content">
                <div class="header-left">
                    <h1><i class="fas fa-microchip"></i> PIC16F887 Sensor Hub</h1>
                    <p>Real-time environmental monitoring system</p>
                </div>
                <div class="header-right">
                    <div class="theme-toggle" onclick="toggleTheme()">
                        <i id="theme-icon" class="fas fa-moon"></i>
                        <span id="theme-text">Dark</span>
                    </div>
                    <div class="connection-indicator">
                        <i id="status-icon" class="fas fa-wifi-slash"></i>
                        <span id="connection-status" class="connection-status disconnected">Disconnected</span>
                    </div>
                    <div class="last-update">
                        <i class="fas fa-clock"></i>
                        <span>Last update: <span id="last-update">--</span></span>
                    </div>
                </div>
            </div>
        </header>
        
        <nav class="tab-navigation">
            <button class="tab-btn active" data-tab="overview">
                <i class="fas fa-tachometer-alt"></i> Overview
            </button>
            <button class="tab-btn" data-tab="charts">
                <i class="fas fa-chart-line"></i> Charts
            </button>
            <button class="tab-btn" data-tab="gauges">
                <i class="fas fa-gauge"></i> Gauges
            </button>
        </nav>
        
        <div id="overview" class="tab-content active">
            <section class="stats-overview">
                <div class="stat-card">
                    <i class="fas fa-thermometer-half"></i>
                    <div class="stat-content">
                        <h3>Avg Temperature</h3>
                        <span id="avg-temp" class="stat-value">--</span>
                    </div>
                </div>
                <div class="stat-card">
                    <i class="fas fa-tint"></i>
                    <div class="stat-content">
                        <h3>Avg Humidity</h3>
                        <span id="avg-humid" class="stat-value">--</span>
                    </div>
                </div>
                <div class="stat-card">
                    <i class="fas fa-sun"></i>
                    <div class="stat-content">
                        <h3>Light Level</h3>
                        <span id="light-level" class="stat-value">--</span>
                    </div>
                </div>
            </section>
            
            <section class="sensor-sections">
                <div class="sensor-section analog-sensors">
                    <h2><i class="fas fa-wave-square"></i> Analog Sensors</h2>
                    <div class="sensor-grid">
                        <div class="sensor-card" data-sensor="lm35">
                            <div class="sensor-header">
                                <i class="fas fa-thermometer-three-quarters"></i>
                                <h3>LM35 Temperature</h3>
                            </div>
                            <div class="sensor-value" id="lm35-value">--</div>
                            <div id="lm35-status" class="sensor-status">--</div>
                        </div>
                        
                        <div class="sensor-card" data-sensor="hih">
                            <div class="sensor-header">
                                <i class="fas fa-tint"></i>
                                <h3>HR202 Humidity</h3>
                            </div>
                            <div class="sensor-value" id="hih-value">--</div>
                            <div id="hih-status" class="sensor-status">--</div>
                        </div>
                        
                        <div class="sensor-card light-card" data-sensor="ldr">
                            <div class="sensor-header">
                                <i class="fas fa-lightbulb"></i>
                                <h3>Light Level (LDR)</h3>
                            </div>
                            <div class="sensor-value" id="ldr-value">--</div>
                            <div class="light-visualization">
                                <div class="light-bar">
                                    <div class="light-indicator" id="light-indicator"></div>
                                </div>
                                <span id="light-percentage">0%</span>
                            </div>
                            <div id="ldr-status" class="sensor-status">--</div>
                        </div>
                    </div>
                </div>
                
                <div class="sensor-section digital-sensors">
                    <h2><i class="fas fa-microchip"></i> Digital Sensors (SHT21)</h2>
                    <div class="sensor-grid">
                        <div class="sensor-card" data-sensor="sht-temp">
                            <div class="sensor-header">
                                <i class="fas fa-thermometer-half"></i>
                                <h3>SHT21 Temperature</h3>
                            </div>
                            <div class="sensor-value" id="sht-temp-value">--</div>
                            <div id="sht-temp-status" class="sensor-status">--</div>
                        </div>
                        
                        <div class="sensor-card" data-sensor="sht-humid">
                            <div class="sensor-header">
                                <i class="fas fa-water"></i>
                                <h3>SHT21 Humidity</h3>
                            </div>
                            <div class="sensor-value" id="sht-humid-value">--</div>
                            <div id="sht-humid-status" class="sensor-status">--</div>
                        </div>
                    </div>
                </div>
            </section>
        </div>
        
        <div id="charts" class="tab-content">
            <section class="charts-section">
                <div class="chart-container">
                    <h3><i class="fas fa-thermometer-half"></i> Temperature History</h3>
                    <canvas id="temperatureChart"></canvas>
                </div>
                <div class="chart-container">
                    <h3><i class="fas fa-tint"></i> Humidity History</h3>
                    <canvas id="humidityChart"></canvas>
                </div>
            </section>
        </div>
        
        <div id="gauges" class="tab-content">
            <section class="gauges-section">
                <div class="gauge-container">
                    <h3>LM35 Temperature</h3>
                    <canvas id="lm35Gauge" width="200" height="200"></canvas>
                </div>
                <div class="gauge-container">
                    <h3>SHT21 Temperature</h3>
                    <canvas id="shtTempGauge" width="200" height="200"></canvas>
                </div>
                <div class="gauge-container">
                    <h3>Light Level</h3>
                    <canvas id="lightGauge" width="200" height="200"></canvas>
                </div>
            </section>
        </div>
        
        <footer class="dashboard-footer">
            <div class="footer-content">
                <p>&copy; 2025 PIC16F887 Sensor Hub - Environmental Monitoring System</p>
            </div>
        </footer>
    </div>
</body>
</html>
)rawliteral";

void setup() {
  // Initialize serial communication
  Serial.begin(BAUD_RATE);
  Serial.setRxBufferSize(SERIAL_SIZE_RX);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize NTP time synchronization
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time with NTP server...");
  
  // Wait for time to be set
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(1000);
    Serial.println("Waiting for NTP time sync...");
    attempts++;
  }
  
  if (getLocalTime(&timeinfo)) {
    Serial.println("Time synchronized successfully");
    Serial.printf("Current time: %02d:%02d:%02d %02d/%02d/%04d\n", 
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                  timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  } else {
    Serial.println("Failed to obtain time from NTP server");
  }
  
  // Set up web server routes
  setupWebServer();
  
  // Start server
  server.begin();
}

void loop() {
  // Read serial data from PIC16F887
  while (Serial.available() > 0) {
    char c = Serial.read();
    serialBuffer += c;
    
    // Check if we have a complete data line (PIC sends simple format now)
    if (c == '\n') {
      isDataComplete = true;
      break;
    }
  }
  
  // Process complete data
  if (isDataComplete) {
    parseSerialData(serialBuffer);
    serialBuffer = "";
    isDataComplete = false;
    sensorData.last_update = millis();
  }
  
  // Send initial time update to PIC only once after startup
  if (!initialTimeSent && millis() > 5000) { // Wait 5 seconds after startup
    sendTimeDataToPIC();
    initialTimeSent = true;
    Serial.println("Initial time data sent to PIC. PIC will handle time incrementing locally.");
  }
}

void parseSerialData(String dataString) {
  // Parse the simplified format from PIC: T1:25.3,H1:65,L:42,T2:24.8,H2:68
  dataString.trim();
  
  // Reset validity flags
  sensorData.lm35_valid = false;
  sensorData.hih_valid = false;
  sensorData.light_valid = false;
  sensorData.sht_temp_valid = false;
  sensorData.sht_humid_valid = false;
  
  // Parse each sensor value
  int startIndex = 0;
  while (startIndex < dataString.length()) {
    int separatorIndex = dataString.indexOf(',', startIndex);
    if (separatorIndex == -1) separatorIndex = dataString.length();
    
    String token = dataString.substring(startIndex, separatorIndex);
    parseSensorToken(token);
    
    startIndex = separatorIndex + 1;
  }
}

void parseSensorToken(String token) {
  int colonIndex = token.indexOf(':');
  if (colonIndex == -1) return;
  
  String sensorType = token.substring(0, colonIndex);
  String valueStr = token.substring(colonIndex + 1);
  
  if (sensorType == "T1" && valueStr != "ERR") {
    sensorData.lm35_temp = valueStr.toFloat();
    sensorData.lm35_valid = true;
  } else if (sensorType == "H1" && valueStr != "ERR") {
    sensorData.hih_humid = valueStr.toFloat();
    sensorData.hih_valid = true;
  } else if (sensorType == "L" && valueStr != "ERR") {
    sensorData.light = valueStr.toFloat();
    sensorData.light_valid = true;
  } else if (sensorType == "T2" && valueStr != "ERR") {
    sensorData.sht_temp = valueStr.toFloat();
    sensorData.sht_temp_valid = true;
  } else if (sensorType == "H2" && valueStr != "ERR") {
    sensorData.sht_humid = valueStr.toFloat();
    sensorData.sht_humid_valid = true;
  }
}

void sendTimeDataToPIC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("TIME:ERROR,DATE:ERROR");
    return;
  }
  
  // Send time and date in format: TIME:HH:MM:SS,DATE:DD/MM/YYYY
  Serial.printf("TIME:%02d:%02d:%02d,DATE:%02d/%02d/%04d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

String getSensorDataJson() {
  // Create JSON document
  StaticJsonDocument<512> doc;
  
  // Add data to JSON
  doc["lm35_temp"] = sensorData.lm35_temp;
  doc["hih_humid"] = sensorData.hih_humid;
  doc["light"] = sensorData.light;
  doc["sht_temp"] = sensorData.sht_temp;
  doc["sht_humid"] = sensorData.sht_humid;
  doc["lm35_valid"] = sensorData.lm35_valid;
  doc["hih_valid"] = sensorData.hih_valid;
  doc["light_valid"] = sensorData.light_valid;
  doc["sht_temp_valid"] = sensorData.sht_temp_valid;
  doc["sht_humid_valid"] = sensorData.sht_humid_valid;
  doc["last_update"] = sensorData.last_update;
  
  // Serialize JSON to string
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

String getCurrentTimeJson() {
  StaticJsonDocument<256> doc;
  struct tm timeinfo;
  
  if (getLocalTime(&timeinfo)) {
    char timeStr[20];
    char dateStr[20];
    
    sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    
    doc["time"] = timeStr;
    doc["date"] = dateStr;
    doc["timestamp"] = mktime(&timeinfo);
    doc["valid"] = true;
  } else {
    doc["time"] = "ERROR";
    doc["date"] = "ERROR";
    doc["timestamp"] = 0;
    doc["valid"] = false;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

void setupWebServer() {
  // Serve the embedded HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", htmlContent);
  });
  
  // API endpoint for sensor data
  server.on("/sensorData", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getSensorDataJson();
    request->send(200, "application/json", json);
  });
  
  // API endpoint for current time
  server.on("/currentTime", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getCurrentTimeJson();
    request->send(200, "application/json", json);
  });
  
  // Handle not found
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
}

void parseJsonData(String jsonString) {
  // Create a JSON document
  DynamicJsonDocument doc(512);
  
  // Parse JSON
  DeserializationError error = deserializeJson(doc, jsonString);
  
  // Test if parsing succeeds
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  
  // Extract values from JSON
  if (doc.containsKey("lm35_temp")) {
    sensorData.lm35_temp = doc["lm35_temp"];
    sensorData.lm35_valid = true;
  }
  
  if (doc.containsKey("hih_humid")) {
    sensorData.hih_humid = doc["hih_humid"];
    sensorData.hih_valid = true;
  }
  
  if (doc.containsKey("light")) {
    sensorData.light = doc["light"];
    sensorData.light_valid = true;
  }
  
  if (doc.containsKey("sht_temp")) {
    if (doc["sht_temp"].is<float>()) {
      sensorData.sht_temp = doc["sht_temp"];
      sensorData.sht_temp_valid = true;
    } else {
      sensorData.sht_temp_valid = false;
    }
  }
  
  if (doc.containsKey("sht_humid")) {
    if (doc["sht_humid"].is<float>()) {
      sensorData.sht_humid = doc["sht_humid"];
      sensorData.sht_humid_valid = true;
    } else {
      sensorData.sht_humid_valid = false;
    }
  }
  
  if (doc.containsKey("event")) {
    String event = doc["event"];
    Serial.print("Event received: ");
    Serial.println(event);
    // Handle events like "alarm_end" if needed
  }
}

