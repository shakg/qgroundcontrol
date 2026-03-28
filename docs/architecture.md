# QGroundControl Architecture Diagram

This document provides a comprehensive visual overview of the QGroundControl software and communication architecture using Mermaid diagrams.

## 1. Overall System Architecture

```mermaid
graph TB
    subgraph UI["UI Layer (QML/Qt Quick)"]
        FlyView["FlyView\n(Flight Display)"]
        PlanView["PlanView\n(Mission Planning)"]
        SetupView["SetupView\n(Vehicle Config)"]
        AnalyzeView["AnalyzeView\n(Logs & Telemetry)"]
        QmlControls["QmlControls\n(Shared Components)"]
    end

    subgraph App["Application Core"]
        QGCApp["QGCApplication\n(App Singleton)"]
        QGCCore["QGCCorePlugin\n(API / Extensibility)"]
        SettingsMgr["SettingsManager"]
    end

    subgraph VehicleMgmt["Vehicle Management"]
        MVMgr["MultiVehicleManager"]
        Vehicle["Vehicle\n(per-vehicle state)"]
        VLinkMgr["VehicleLinkManager"]
        ParamMgr["ParameterManager"]
        FactSys["Fact System\n(FactGroups / Facts)"]
    end

    subgraph MissionSys["Mission & Planning"]
        MissionCtrl["MissionController"]
        MissionMgr["MissionManager"]
        GeoFenceMgr["GeoFenceManager"]
        RallyPtMgr["RallyPointManager"]
        TerrainSys["Terrain System"]
    end

    subgraph CommLayer["Communication Layer"]
        LinkMgr["LinkManager"]
        MAVLink["MAVLinkProtocol"]
        subgraph Links["Link Types"]
            Serial["SerialLink"]
            UDP["UDPLink"]
            TCP["TCPLink"]
            BLE["BluetoothLink"]
            LogReplay["LogReplayLink"]
            MockLink["MockLink (Test)"]
        end
    end

    subgraph Plugins["Plugin System"]
        FirmwarePluginMgr["FirmwarePluginManager"]
        PX4Plugin["PX4FirmwarePlugin"]
        APMPlugin["APMFirmwarePlugin"]
        AutoPilotPlugins["AutoPilotPlugins\n(PX4 / APM / Generic)"]
    end

    subgraph Subsystems["Subsystems"]
        VideoMgr["VideoManager\n(GStreamer/RTSP)"]
        JoystickMgr["JoystickManager\n(SDL2)"]
        PosMgr["PositionManager\n(Follow-Me / GPS)"]
        GPSRtk["GPS/RTK/NTRIP"]
        ADSBMgr["ADSBVehicleManager"]
        CamMgr["CameraManager"]
        GimbalCtrl["GimbalController"]
        RemoteID["RemoteIDManager"]
    end

    subgraph Physical["Physical / Network Layer"]
        UART["UART / USB Serial"]
        WiFiUDP["Wi-Fi UDP"]
        LTE_TCP["LTE / TCP"]
        BTRadio["Bluetooth Radio"]
        Drone["Drone / Autopilot\n(PX4 / ArduPilot)"]
    end

    %% App Core connections
    QGCApp --> MVMgr
    QGCApp --> LinkMgr
    QGCApp --> SettingsMgr
    QGCApp --> QGCCore

    %% UI -> Core
    FlyView --> MVMgr
    FlyView --> VideoMgr
    FlyView --> JoystickMgr
    PlanView --> MissionCtrl
    SetupView --> AutoPilotPlugins
    AnalyzeView --> MAVLink

    %% Vehicle Management
    MVMgr --> Vehicle
    Vehicle --> VLinkMgr
    Vehicle --> ParamMgr
    Vehicle --> FactSys
    Vehicle --> MissionMgr
    Vehicle --> GeoFenceMgr
    Vehicle --> RallyPtMgr
    Vehicle --> CamMgr
    Vehicle --> GimbalCtrl
    Vehicle --> RemoteID

    %% Mission
    MissionCtrl --> MissionMgr
    MissionCtrl --> TerrainSys

    %% Plugins
    Vehicle --> FirmwarePluginMgr
    FirmwarePluginMgr --> PX4Plugin
    FirmwarePluginMgr --> APMPlugin
    Vehicle --> AutoPilotPlugins

    %% Communication
    VLinkMgr --> LinkMgr
    LinkMgr --> MAVLink
    LinkMgr --> Serial
    LinkMgr --> UDP
    LinkMgr --> TCP
    LinkMgr --> BLE
    LinkMgr --> LogReplay
    LinkMgr --> MockLink

    %% Physical
    Serial --> UART
    UDP --> WiFiUDP
    TCP --> LTE_TCP
    BLE --> BTRadio
    UART --> Drone
    WiFiUDP --> Drone
    LTE_TCP --> Drone
    BTRadio --> Drone

    %% Subsystems
    PosMgr --> GPSRtk
    ADSBMgr --> FlyView
    QGCApp --> JoystickMgr
    QGCApp --> VideoMgr
    QGCApp --> PosMgr
    QGCApp --> ADSBMgr
```

---

## 2. Communication Layer Detail

```mermaid
graph LR
    subgraph GCS["Ground Control Station"]
        LinkMgr["LinkManager\n• Creates/manages links\n• MAVLink channel alloc\n• Auto-connect logic\n• mDNS/Zeroconf discovery"]

        subgraph LinkTypes["Link Implementations"]
            direction TB
            SL["SerialLink\n(UART/USB)\nBaud, parity, flow-ctrl"]
            UL["UDPLink\n(Wi-Fi / LTE)\nMulti-host, multicast"]
            TL["TCPLink\n(LTE / Internet)\nHost + port"]
            BL["BluetoothLink\n(Classic + BLE)"]
            LRL["LogReplayLink\n(Offline replay)"]
            ML["MockLink\n(Simulation / Test)"]
        end

        MAVProto["MAVLinkProtocol\n• Parses MAVLink frames\n• Channel multiplexing\n• Telemetry logging\n• Message forwarding\n• Loss rate calculation"]

        Signing["MAVLinkSigning\n(Auth / Security)"]
        StreamCfg["MAVLinkStreamConfig\n(Rate configuration)"]
    end

    subgraph VehicleSide["Vehicle Side"]
        VLinkMgr["VehicleLinkManager\n• Per-vehicle links\n• Failover handling"]
        MsgHandlers["Message Handlers\n• HEARTBEAT\n• ATTITUDE\n• GLOBAL_POSITION_INT\n• SYS_STATUS\n• BATTERY_STATUS\n• PARAM_VALUE\n• MISSION_ITEM\n• COMMAND_ACK\n• ..."]
    end

    LinkMgr --> SL & UL & TL & BL & LRL & ML
    SL & UL & TL & BL --> MAVProto
    MAVProto --> Signing
    MAVProto --> StreamCfg
    MAVProto -- "messageReceived signal" --> VLinkMgr
    VLinkMgr --> MsgHandlers
```

---

## 3. MAVLink Message Flow

```mermaid
sequenceDiagram
    participant Drone as Drone (Autopilot)
    participant Link as LinkInterface<br/>(Serial/UDP/TCP/BT)
    participant Proto as MAVLinkProtocol
    participant Vehicle as Vehicle
    participant FactGroup as FactGroup / Facts
    participant QML as QML UI

    Drone->>Link: Raw bytes (MAVLink frame)
    Link->>Proto: bytesReceived signal
    Proto->>Proto: mavlink_parse_char() decode
    Proto->>Vehicle: messageReceived signal
    Vehicle->>Vehicle: _handleIncomingMessage()
    Vehicle->>FactGroup: Update values (position, battery, GPS...)
    FactGroup->>QML: Q_PROPERTY notify signals
    QML->>QML: UI refresh

    Note over QML,Drone: Command Path (GCS → Drone)
    QML->>Vehicle: setArmed(true) / sendCommand()
    Vehicle->>Proto: MAV_CMD_COMPONENT_ARM_DISARM
    Proto->>Link: writeBytesThreadSafe()
    Link->>Drone: Raw bytes (MAVLink frame)
    Drone->>Vehicle: COMMAND_ACK (MAV_RESULT)
    Vehicle->>QML: armedChanged signal
```

---

## 4. Vehicle State & Fact System

```mermaid
graph TD
    subgraph Vehicle["Vehicle (per-drone instance)"]
        VS["Vehicle State\n_id, _armed, _flightMode\n_latitude, _longitude\n_altitude, _heading\n_rssi, _batteryVoltage"]

        subgraph FG["Fact Groups"]
            GPS_FG["VehicleGPSFactGroup\nfix_type, lat, lon, alt\nhdop, vdop, count"]
            BAT_FG["VehicleBatteryFactGroup\nvoltage, current, percent\ntemperature, remaining"]
            WIND_FG["VehicleWindFactGroup\ndirection, speed, vert"]
            VIB_FG["VehicleVibrationFactGroup\nvibration_x/y/z, clipping"]
            TEMP_FG["VehicleTemperatureFactGroup\ntemperature1/2/3"]
            DIST_FG["VehicleDistanceSensorFactGroup\nrotationNone, rotationYaw45..."]
            TERRAIN_FG["TerrainFactGroup\nterrainAltitude, distanceToTerrain"]
        end

        subgraph Managers["Component Managers"]
            ParamMgr["ParameterManager\n• Sync params w/ vehicle\n• Disk caching\n• Load progress"]
            MissionMgr["MissionManager\n• Upload / download\n• Item tracking"]
            FTPMgr["FTPManager\n• Log downloads\n• File transfers"]
            CamMgr["CameraManager\n• Camera control\n• XML definitions"]
            GimbalCtrl["GimbalController\n• Pan / tilt / roll"]
            RemoteID["RemoteIDManager\n• FAA remote ID"]
            AutotuneMgr["AutotuneManager\n• PID autotuning"]
        end

        InitSM["InitialConnectStateMachine\n1. Request parameters\n2. Component info\n3. Load autopilot plugin\n4. Waypoint protocol\n5. Home location\n6. Mission items\n7. Vehicle ready"]
    end

    VS --> FG
    VS --> Managers
    VS --> InitSM
```

---

## 5. Plugin Architecture

```mermaid
graph TB
    subgraph FirmwarePlugins["Firmware Plugin Layer"]
        FPM["FirmwarePluginManager\n(Factory + Registry)"]
        FPBase["FirmwarePlugin (Base)\n• Flight mode definitions\n• Capability flags\n• Custom commands\n• Parameter remapping\n• Guided mode ops\n• Takeoff / landing"]
        PX4FP["PX4FirmwarePlugin\n• PX4 flight modes\n• PX4 custom commands\n• PX4 param names"]
        APMFP["APMFirmwarePlugin\n• ArduPilot modes\n• APM-specific UI\n• APM parameters"]
    end

    subgraph AutoPilotPlugins["AutoPilot Plugin Layer (Setup UI)"]
        APBase["AutoPilotPlugin (Base)\n• Vehicle component list\n• Setup pages"]
        PX4AP["PX4AutoPilotPlugin\n• Sensor calibration\n• Flight modes\n• Safety/failsafe\n• Radio setup"]
        APMAP["APMAutoPilotPlugin\n• APM sensors\n• APM modes\n• APM failsafe"]
        GenAP["GenericAutoPilotPlugin\n• Basic setup\n• Parameter editor"]
        VehicleComp["VehicleComponent\n• Sensors\n• Radio\n• FlightModes\n• Safety\n• Power\n• Motors"]
    end

    subgraph API["Core API / Extensibility"]
        CorePlugin["QGCCorePlugin\n• Custom pages\n• Custom toolbar\n• App settings\n• Instrument pages"]
        QGCOpts["QGCOptions\n• Fly view options\n• UI customization"]
    end

    FPM --> FPBase
    FPBase --> PX4FP
    FPBase --> APMFP
    APBase --> PX4AP
    APBase --> APMAP
    APBase --> GenAP
    PX4AP --> VehicleComp
    APMAP --> VehicleComp
    CorePlugin --> QGCOpts
```

---

## 6. UI Component Hierarchy

```mermaid
graph TD
    MW["MainWindow.qml\n(Root)"]

    MW --> Header["ToolBar / Header\n• Connection status\n• Battery indicator\n• GPS indicator\n• Vehicle selector"]
    MW --> Views["View Router"]
    MW --> StatusBar["Status Bar\n• Flight time\n• Home distance\n• Altitude"]

    Views --> FV["FlyView\n(Live flight)"]
    Views --> PV["PlanView\n(Mission editor)"]
    Views --> SV["SetupView\n(Configuration)"]
    Views --> AV["AnalyzeView\n(Logs / Analysis)"]

    FV --> FlightDisp["FlightDisplayView\n• Video feed\n• Instrument overlay\n• Attitude indicator"]
    FV --> FlightMap["FlightMap\n• Vehicle track\n• Waypoint overlay\n• ADSB traffic"]
    FV --> ToolStrip["FlyViewToolStrip\n• Arm/Disarm\n• Takeoff/Land\n• RTL / Guided"]
    FV --> VJoy["VirtualJoystick"]
    FV --> GuidedAction["GuidedActionConfirm\n• Action dialogs"]
    FV --> MultiVeh["MultiVehicleList"]

    PV --> MissionEditor["Mission Editor\n• Waypoints\n• Survey patterns\n• Corridor scan\n• Structure scan"]
    PV --> GeoFenceEditor["GeoFence Editor"]
    PV --> RallyEditor["Rally Points Editor"]
    PV --> PlanToolBar["Plan ToolBar\n• Upload / Download\n• Clear / File ops"]

    SV --> Summary["Vehicle Summary"]
    SV --> FirmwareUpg["Firmware Upgrade"]
    SV --> SensorCal["Sensor Calibration"]
    SV --> RadioCal["Radio Calibration"]
    SV --> FlightModesCfg["Flight Modes Config"]

    AV --> MAVConsole["MAVLink Console"]
    AV --> MAVInspector["MAVLink Inspector"]
    AV --> LogDownload["Log Download"]
    AV --> TelemetryChart["Telemetry Charts"]
```

---

## 7. Subsystem Overview

```mermaid
graph LR
    subgraph VideoSys["Video System"]
        VideoMgr["VideoManager\n• Stream config\n• Source selection"]
        VideoRx["VideoReceiver\n• GStreamer pipeline\n• RTSP / MJPEG\n• Recording"]
        VideoDisp["VideoDisplay\n(QML overlay)"]
        VideoMgr --> VideoRx --> VideoDisp
    end

    subgraph InputSys["Input / Joystick"]
        JoyMgr["JoystickManager\n(SDL2 backend)"]
        JoyDev["Joystick\n• Axis → flight ctrl\n• Button → commands\n• Calibration"]
        JoyMgr --> JoyDev
    end

    subgraph PositionSys["Position / GPS"]
        PosMgr["PositionManager\n(Device location)"]
        GPSProv["GPSProvider\n(Local GPS device)"]
        GPSRTK["GPSRtk\n(RTK-GNSS)"]
        NTRIP["NTRIP Client\n(RTK corrections)"]
        PosMgr --> GPSProv
        GPSRTK --> NTRIP
    end

    subgraph TerrainSys["Terrain System"]
        TerrainTM["TerrainTileManager\n• Downloads tiles\n• Disk cache"]
        TerrainQ["TerrainQuery\n• Altitude queries"]
        TerrainTile["TerrainTile\n(Elevation data)"]
        TerrainTM --> TerrainTile
        TerrainQ --> TerrainTM
    end

    subgraph ADSBSys["ADSB Traffic"]
        ADSBMgr["ADSBVehicleManager"]
        ADSBLink["ADSBTCPLink\n(ADS-B receiver)"]
        ADSBVeh["ADSBVehicle\n(Tracked aircraft)"]
        ADSBMgr --> ADSBLink
        ADSBMgr --> ADSBVeh
    end
```

---

## 8. Threading Model

```mermaid
graph TB
    subgraph MainThread["Main Thread (Qt Event Loop)"]
        QML_T["QML Engine\n(UI rendering)"]
        Signals["Qt Signal/Slot\ndispatch"]
        AppLogic["Application Logic\n(QGCApplication, Managers)"]
    end

    subgraph LinkThreads["Link Worker Threads (per link)"]
        SerialWorker["SerialWorker\n(QSerialPort I/O)"]
        UDPWorker["UDPWorker\n(QUdpSocket I/O)"]
        TCPWorker["TCPWorker\n(QTcpSocket I/O)"]
        BTWorker["BluetoothWorker\n(BT I/O)"]
    end

    subgraph OtherThreads["Other Worker Threads"]
        FileThread["File I/O Thread\n(param cache, logs)"]
        TerrainThread["Terrain Tile\nDownload Thread"]
        VideoThread["GStreamer Thread\n(video decode)"]
    end

    MainThread <-- "Qt queued signals\n(thread-safe)" --> LinkThreads
    MainThread <-- "Qt queued signals" --> OtherThreads

    note["writeBytesThreadSafe()\nqueues bytes to worker\nthread via Qt signals"]
```

---

## 9. Settings & Persistence

```mermaid
graph TD
    SM["SettingsManager\n(Central Registry)"]

    SM --> AppSettings["AppSettings\n• Language / Locale\n• Units (metric/imperial)\n• Display options\n• Audio alerts"]
    SM --> ConnSettings["ConnectSettings\n• Auto-connect\n• Baud rates\n• Serial port config"]
    SM --> VideoSettings["VideoSettings\n• Stream source\n• RTSP URL\n• Recording path"]
    SM --> MapSettings["MapSettings\n• Map provider\n• Offline tiles\n• Zoom levels"]
    SM --> JoySettings["JoystickSettings\n• Axis mapping\n• Button mapping\n• Calibration"]
    SM --> NTRIPSettings["NTRIPSettings\n• Server / credentials\n• Mount point"]
    SM --> RemoteIDSettings["RemoteIDSettings\n• Operator ID\n• Aircraft ID"]
    SM --> FWSettings["FirmwareSettings\n• Stable/dev channel\n• Custom URL"]

    SM --> QSettings["QSettings\n(Platform storage)\nLinux: ~/.config/QGroundControl\nmacOS: ~/Library/Preferences\nWindows: Registry"]

    ParamCache["Parameter Cache\n(per vehicle ID+component)\nJSON on disk"]
    LinkConfigs["Link Configurations\n(saved connections)\nJSON on disk"]

    SM --> ParamCache
    SM --> LinkConfigs
```
