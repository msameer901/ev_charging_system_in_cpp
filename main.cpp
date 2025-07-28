#include <iostream>
#include <cstring>
#include <queue>
#include <vector>
#include <iomanip>
using namespace std;

// Constants
const int MAX_USERS = 10;
const int MAX_DOCKS = 5;
const int MAX_BOOKINGS = 20;
const int MAX_STATIONS = 3;
const float GRID_CAPACITY = 150.0;

// Charging dock types
const int SLOW = 7;
const int MEDIUM = 22;
const int FAST = 50;
const int SOLAR = 7;

// Peak hours
const float PEAK_START = 12.0;
const float PEAK_END = 18.0;

// CO2 emission factor for grid energy (kg CO2/kWh)
const float CO2_GRID_FACTOR = 0.5;

// Weather conditions affecting solar output
enum WeatherCondition { SUNNY, CLOUDY, NIGHT };
WeatherCondition currentWeather = SUNNY;

// Structure for queued bookings
struct QueuedBooking {
    int userID;
    int vehicleID;
    float startTime;
    float duration;
    int powerRating;
    int chargingType;
    QueuedBooking(int uID, int vID, float sTime, float dur, int pRating, int cType)
        : userID(uID), vehicleID(vID), startTime(sTime), duration(dur), powerRating(pRating), chargingType(cType) {}
};

// Base class for EnergySource
class EnergySource {
public:
    virtual float getRateAdjustment() const = 0;
    virtual float getCO2Emission(float energy) const = 0;
    virtual float getAvailablePower(float basePower) const = 0;
    virtual string getSourceName() const = 0;
    virtual ~EnergySource() {}
};

// Derived class for GridPower
class GridPower : public EnergySource {
public:
    float getRateAdjustment() const override { return 1.0; }
    float getCO2Emission(float energy) const override { return energy * CO2_GRID_FACTOR; }
    float getAvailablePower(float basePower) const override { return basePower; }
    string getSourceName() const override { return "Grid"; }
};

// Derived class for SolarPower
class SolarPower : public EnergySource {
public:
    float getRateAdjustment() const override { return 0.9; }
    float getCO2Emission(float energy) const override { return 0.0; }
    float getAvailablePower(float basePower) const override {
        switch (currentWeather) {
            case SUNNY: return basePower;
            case CLOUDY: return basePower * 0.5f;
            case NIGHT: return 0.0f;
            default: return basePower;
        }
    }
    string getSourceName() const override { return "Solar"; }
};

// User class
class User {
public:
    int userID;
    char name[50];
    bool isRegistered;
    int membershipLevel;

    User() : userID(-1), isRegistered(false), membershipLevel(0) {
        name[0] = '\0';
    }

    void registerUser(int id, const char* userName, int level) {
        if (level != 0 && level != 1) level = 0;
        userID = id;
        strncpy(name, userName, 49);
        name[49] = '\0';
        isRegistered = true;
        membershipLevel = level;
    }
};

// Electric Vehicle class
class EV {
public:
    int vehicleID;
    int userID;
    float batterySOC;
    float batteryCapacity;
    bool supportsV2G;

    EV() : vehicleID(-1), userID(-1), batterySOC(0.0f), batteryCapacity(0.0f), supportsV2G(false) {}

    void registerVehicle(int vID, int uID, float soc, float capacity, bool v2g) {
        vehicleID = vID;
        userID = uID;
        batterySOC = max(0.0f, min(100.0f, soc));
        batteryCapacity = max(0.0f, capacity);
        supportsV2G = v2g;
    }

    float dischargeToGrid(float energy) {
        if (!supportsV2G) return 0.0f;
        float energyAvailable = (batterySOC / 100.0f) * batteryCapacity;
        float energyToDischarge = min(energy, energyAvailable);
        batterySOC -= (energyToDischarge / batteryCapacity) * 100.0f;
        if (batterySOC < 0.0f) batterySOC = 0.0f;
        return energyToDischarge;
    }
};

// Charging Dock class
class ChargingDock {
public:
    int dockID;
    int powerRating;
    bool isOccupied;
    int currentVehicleID;
    EnergySource* energySource;

    ChargingDock() : dockID(-1), powerRating(SLOW), isOccupied(false), currentVehicleID(-1), energySource(nullptr) {}

    // Disable copy constructor and assignment operator to avoid shallow copy
    ChargingDock(const ChargingDock&) = delete;
    ChargingDock& operator=(const ChargingDock&) = delete;

    ~ChargingDock() { delete energySource; }

    void initialize(int id, int rating, EnergySource* source) {
        dockID = id;
        powerRating = rating;
        isOccupied = false;
        currentVehicleID = -1;
        delete energySource; // release previous if any
        energySource = source;
    }
};

// Booking class
class Booking {
public:
    int bookingID;
    int userID;
    int vehicleID;
    int dockID;
    int stationID;
    float startTime;
    float duration;
    bool isActive;
    float cost;
    float energyConsumed;
    int chargingType;

    Booking() : bookingID(-1), userID(-1), vehicleID(-1), dockID(-1), stationID(-1), startTime(0.0f),
                duration(0.0f), isActive(false), cost(0.0f), energyConsumed(0.0f), chargingType(0) {}

    void createBooking(int bID, int uID, int vID, int dID, int sID, float time, float dur, int type) {
        bookingID = bID;
        userID = uID;
        vehicleID = vID;
        dockID = dID;
        stationID = sID;
        startTime = time;
        duration = dur;
        isActive = true;
        cost = 0.0f;
        energyConsumed = 0.0f;
        chargingType = type;
    }

    void cancelBooking() {
        isActive = false;
    }
};

// Charging Station class
class ChargingStation {
public:
    ChargingDock docks[MAX_DOCKS];
    User users[MAX_USERS];
    EV vehicles[MAX_USERS];
    Booking bookings[MAX_BOOKINGS];
    int userCount;
    int vehicleCount;
    int bookingCount;
    float totalOccupiedTime[MAX_DOCKS];
    float systemStartTime;
    queue<QueuedBooking> bookingQueue;
    int stationID;

    // Disable copy constructor and assignment operator to prevent shallow copy issues
    ChargingStation(const ChargingStation&) = delete;
    ChargingStation& operator=(const ChargingStation&) = delete;

    ChargingStation(int sID = 1) : userCount(0), vehicleCount(0), bookingCount(0), systemStartTime(0.0f), stationID(sID) {
        for (int i = 0; i < MAX_DOCKS; i++) totalOccupiedTime[i] = 0.0f;
        docks[0].initialize(1, SLOW, new GridPower());
        docks[1].initialize(2, SLOW, new SolarPower());
        docks[2].initialize(3, MEDIUM, new GridPower());
        docks[3].initialize(4, MEDIUM, new SolarPower());
        docks[4].initialize(5, FAST, new GridPower());
    }

    ~ChargingStation() {}

    void notifyUser(int userID, const string& msg, float value = -1.0f) {
        cout << "\n[Notification for User ID: " << userID << "] " << msg;
        if (value >= 0.0f) cout << " " << value;
        cout << endl;
    }

    bool registerUser(int id, const char* name, int level) {
        if (userCount >= MAX_USERS) {
            cout << "Maximum user limit reached!" << endl;
            return false;
        }
        for (int i = 0; i < userCount; i++) {
            if (users[i].userID == id) {
                cout << "User ID already exists!" << endl;
                return false;
            }
        }
        users[userCount].registerUser(id, name, level);
        userCount++;
        cout << "User registered successfully! Station ID: " << stationID << endl;
        return true;
    }

    bool registerVehicle(int vID, int uID, float soc, float capacity, bool v2g) {
        if (vehicleCount >= MAX_USERS) {
            cout << "Maximum vehicle limit reached!" << endl;
            return false;
        }
        for (int i = 0; i < userCount; i++) {
            if (users[i].userID == uID && users[i].isRegistered) {
                for (int j = 0; j < vehicleCount; j++) {
                    if (vehicles[j].vehicleID == vID) {
                        cout << "Vehicle ID already exists!" << endl;
                        return false;
                    }
                }
                vehicles[vehicleCount].registerVehicle(vID, uID, soc, capacity, v2g);
                vehicleCount++;
                cout << "Vehicle registered successfully! Station ID: " << stationID << endl;
                return true;
            }
        }
        cout << "User not found!" << endl;
        return false;
    }

    bool isCriticalBooking(int uID, int vID) {
        bool isPremium = false;
        float soc = 0.0f;
        for (int i = 0; i < userCount; i++) {
            if (users[i].userID == uID && users[i].membershipLevel == 1) {
                isPremium = true;
                break;
            }
        }
        for (int i = 0; i < vehicleCount; i++) {
            if (vehicles[i].vehicleID == vID) {
                soc = vehicles[i].batterySOC;
                break;
            }
        }
        return isPremium || soc < 20.0f;
    }

    bool isDockAvailable(int dockID, float startTime, float duration) {
        if (bookingCount < 0 || bookingCount > MAX_BOOKINGS) {
            cout << "[ERROR] Invalid bookingCount: " << bookingCount << endl;
            return false;
        }
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].isActive && bookings[i].dockID == dockID) {
                float bStart = bookings[i].startTime;
                float bEnd = bStart + bookings[i].duration;
                float newEnd = startTime + duration;
                if (startTime < bEnd && newEnd > bStart) {
                    return false;
                }
            }
        }
        return true;
    }

    int findAvailableDock(int powerRating, float startTime, float duration, bool isSolarCharging) {
        bool isPeakHour = (startTime >= PEAK_START && startTime < PEAK_END);
        vector<int> suitableDocks;

        for (int i = 0; i < MAX_DOCKS; i++) {
            if (docks[i].energySource == nullptr) {
                continue;
            }
            float availablePower = docks[i].energySource->getAvailablePower(docks[i].powerRating);
            if (!docks[i].isOccupied && availablePower >= powerRating &&
                (!isSolarCharging || dynamic_cast<SolarPower*>(docks[i].energySource)) &&
                isDockAvailable(docks[i].dockID, startTime, duration)) {
                suitableDocks.push_back(docks[i].dockID);
            }
        }

        if (suitableDocks.empty()) {
            return -1;
        }

        if (isPeakHour && !isSolarCharging) {
            for (int dockID : suitableDocks) {
                for (int i = 0; i < MAX_DOCKS; i++) {
                    if (docks[i].dockID == dockID && dynamic_cast<SolarPower*>(docks[i].energySource)) {
                        return dockID;
                    }
                }
            }
        }
        return suitableDocks[0];
    }

    float getCurrentPowerConsumption() {
        float totalPower = 0.0f;
        for (int i = 0; i < MAX_DOCKS; i++) {
            if (docks[i].isOccupied && docks[i].energySource != nullptr) {
                totalPower += docks[i].energySource->getAvailablePower(docks[i].powerRating);
            }
        }
        return totalPower;
    }

    bool createBooking(int uID, int vID, float startTime, float duration, int powerRating, int chargingType) {
        if (bookingCount < 0 || bookingCount >= MAX_BOOKINGS) {
            cout << "Maximum booking limit reached or invalid bookingCount!" << endl;
            return false;
        }
        if (startTime < 0.0f || startTime >= 24.0f || duration <= 0.0f) {
            cout << "Invalid start time or duration!" << endl;
            return false;
        }

        bool userExists = false, vehicleExists = false;
        for (int i = 0; i < userCount; i++) {
            if (users[i].userID == uID && users[i].isRegistered) {
                userExists = true;
                break;
            }
        }
        for (int i = 0; i < vehicleCount; i++) {
            if (vehicles[i].vehicleID == vID && vehicles[i].userID == uID) {
                vehicleExists = true;
                break;
            }
        }
        if (!userExists || !vehicleExists) {
            cout << "User or vehicle not found!" << endl;
            return false;
        }

        if (bookingCount == 0) systemStartTime = startTime;

        bool isPeakHour = (startTime >= PEAK_START && startTime < PEAK_END);
        float adjustedStartTime = startTime;
        if (isPeakHour && !isCriticalBooking(uID, vID)) {
            adjustedStartTime = PEAK_END;
            notifyUser(uID, "Your booking has been deferred due to peak hours. New start time:", adjustedStartTime);
        }

        bool isSolarCharging = (chargingType == 4);
        int dockID = findAvailableDock(powerRating, adjustedStartTime, duration, isSolarCharging);
        if (dockID == -1) {
            cout << "No available dock. Booking cannot be created." << endl;
            return false;
        }

        bookings[bookingCount].createBooking(bookingCount + 1, uID, vID, dockID, stationID, adjustedStartTime, duration, chargingType);
        for (int i = 0; i < MAX_DOCKS; i++) {
            if (docks[i].dockID == dockID) {
                docks[i].isOccupied = true;
                docks[i].currentVehicleID = vID;
                break;
            }
        }
        bookingCount++;
        notifyUser(uID, "Upcoming charging session scheduled at:", adjustedStartTime);
        cout << "Booking created successfully! Booking ID: " << bookingCount << endl;
        return true;
    }

    void cancelBooking(int bookingID) {
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].bookingID == bookingID && bookings[i].isActive) {
                float penalty = 0.0f;
                float timeToStart = bookings[i].startTime - systemStartTime;
                if (timeToStart < 1.0f) penalty = 5.0f;
                else if (timeToStart < 4.0f) penalty = 2.0f;
                bookings[i].cancelBooking();
                for (int j = 0; j < MAX_DOCKS; j++) {
                    if (docks[j].dockID == bookings[i].dockID) {
                        docks[j].isOccupied = false;
                        docks[j].currentVehicleID = -1;
                        break;
                    }
                }
                notifyUser(bookings[i].userID, "Booking cancelled. Penalty charged: $", penalty);
                break;
            }
        }
    }

    void processQueue() {
        while (!bookingQueue.empty()) {
            QueuedBooking qb = bookingQueue.front();
            if (createBooking(qb.userID, qb.vehicleID, qb.startTime, qb.duration, qb.powerRating, qb.chargingType)) {
                bookingQueue.pop();
            } else {
                break;
            }
        }
    }

    void completeBooking(int bookingID) {
    for (int i = 0; i < bookingCount; i++) {
        if (bookings[i].bookingID == bookingID && bookings[i].isActive) {
            bookings[i].cancelBooking();
            int dockIndex = -1;
            for (int j = 0; j < MAX_DOCKS; j++) {
                if (docks[j].dockID == bookings[i].dockID) {
                    docks[j].isOccupied = false;
                    docks[j].currentVehicleID = -1;
                    dockIndex = j;
                    break;
                }
            }
            if (dockIndex == -1 || docks[dockIndex].energySource == nullptr) {
                cout << "Error: Invalid dock or energy source!" << endl;
                return;
            }
            float energy = docks[dockIndex].energySource->getAvailablePower(docks[dockIndex].powerRating) * bookings[i].duration;
            bookings[i].energyConsumed = energy;
            totalOccupiedTime[dockIndex] += bookings[i].duration;

            float ratePerKWh = 0.0f;
            if (bookings[i].chargingType == 1) ratePerKWh = 0.2f; // Slow
            else if (bookings[i].chargingType == 2) ratePerKWh = 0.3f; // Medium
            else if (bookings[i].chargingType == 3) ratePerKWh = 0.4f; // Fast
            else if (bookings[i].chargingType == 4) ratePerKWh = 0.15f; // Solar

            // Apply a discount for solar charging
            if (bookings[i].chargingType == 4) {
                ratePerKWh *= 0.85f; // 15% discount for solar charging
            }

            if (bookings[i].startTime >= PEAK_START && bookings[i].startTime < PEAK_END) {
                ratePerKWh *= 1.2f; // Peak hour surcharge
            }
            ratePerKWh *= docks[dockIndex].energySource->getRateAdjustment();

            float cost = energy * ratePerKWh;
            for (int j = 0; j < userCount; j++) {
                if (users[j].userID == bookings[i].userID && users[j].membershipLevel == 1) {
                    cost *= 0.85f; // 15% discount for premium members
                    break;
                }
            }
            bookings[i].cost = cost;

            for (int j = 0; j < vehicleCount; j++) {
                if (vehicles[j].vehicleID == bookings[i].vehicleID) {
                    vehicles[j].batterySOC += (energy / vehicles[j].batteryCapacity) * 100.0f;
                    if (vehicles[j].batterySOC > 100.0f) vehicles[j].batterySOC = 100.0f;
                    break;
                }
            }

            cout << "Invoice for Booking ID: " << bookingID << endl;
            cout << "User  ID: " << bookings[i].userID << endl;
            cout << "Vehicle ID: " << bookings[i].vehicleID << endl;
            cout << "Energy Consumed: " << energy << " kWh" << endl;
            cout << "Charging Rate: $" << ratePerKWh << " per kWh" << endl;
            cout << "Total Cost: $" << cost << endl;

            notifyUser (bookings[i].userID, "Charging session completed. Energy consumed:", energy);
            notifyUser (bookings[i].userID, "Total cost for the session: $", cost);

            break;
        }
    }
}

    void displayRealTimeData() {
        cout << "\n=== Real-Time Charging Data ===\n";
        bool activeFound = false;
        // For demonstration, consider current time = systemStartTime + 1.0 to simulate elapsed time
        float currentTime = systemStartTime + 1.0f;

        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].isActive) {
                activeFound = true;
                float elapsedTime = currentTime - bookings[i].startTime;
                if (elapsedTime < 0.0f) elapsedTime = 0.0f;
                if (elapsedTime > bookings[i].duration) elapsedTime = bookings[i].duration;

                int dockIndex = -1;
                for (int j = 0; j < MAX_DOCKS; j++) {
                    if (docks[j].dockID == bookings[i].dockID) {
                        dockIndex = j;
                        break;
                    }
                }
                if (dockIndex == -1 || docks[dockIndex].energySource == nullptr) {
                    cout << "Error: Invalid dock for booking " << bookings[i].bookingID << endl;
                    continue;
                }
                float energySoFar = docks[dockIndex].energySource->getAvailablePower(docks[dockIndex].powerRating) * elapsedTime;
                float remainingTime = bookings[i].duration - elapsedTime;
                cout << "Booking ID: " << bookings[i].bookingID << endl;
                cout << "Vehicle ID: " << bookings[i].vehicleID << endl;
                cout << "Energy Delivered: " << energySoFar << " kWh" << endl;
                cout << "Remaining Time: " << remainingTime << " hours" << endl;
                cout << "------------------------" << endl;
            }
        }
        if (!activeFound) {
            cout << "No active bookings." << endl;
        }
    }

    void generateReport() {
        cout << "\n=== Charging Station Analytics Report ===\n";
        float totalSystemTime = 0.0f;
        float totalOccupied = 0.0f;
        float latestEndTime = systemStartTime;
        for (int i = 0; i < bookingCount; i++) {
            float endTime = bookings[i].startTime + bookings[i].duration;
            if (endTime > latestEndTime) latestEndTime = endTime;
        }
        if (bookingCount > 0) totalSystemTime = latestEndTime - systemStartTime;
        for (int i = 0; i < MAX_DOCKS; i++) totalOccupied += totalOccupiedTime[i];
        float utilization = (totalSystemTime > 0.0f) ? (totalOccupied / (totalSystemTime * MAX_DOCKS)) * 100.0f : 0.0f;
        cout << "Station Utilization: " << utilization << "%" << endl;

        float totalDuration = 0.0f;
        int completedBookings = 0;
        for (int i = 0; i < bookingCount; i++) {
            if (!bookings[i].isActive) {
                totalDuration += bookings[i].duration;
                completedBookings++;
            }
        }
        float avgDuration = (completedBookings > 0) ? totalDuration / completedBookings : 0.0f;
        cout << "Average Session Duration: " << avgDuration << " hours" << endl;

        float gridEnergy = 0.0f, solarEnergy = 0.0f;
        for (int i = 0; i < bookingCount; i++) {
            if (!bookings[i].isActive) {
                for (int j = 0; j < MAX_DOCKS; j++) {
                    if (docks[j].dockID == bookings[i].dockID && docks[j].energySource != nullptr) {
                        if (dynamic_cast<GridPower*>(docks[j].energySource)) {
                            gridEnergy += bookings[i].energyConsumed;
                        } else {
                            solarEnergy += bookings[i].energyConsumed;
                        }
                        break;
                    }
                }
            }
        }
        float totalEnergy = gridEnergy + solarEnergy;
        float gridRatio = (totalEnergy > 0.0f) ? (gridEnergy / totalEnergy) * 100.0f : 0.0f;
        float solarRatio = (totalEnergy > 0.0f) ? (solarEnergy / totalEnergy) * 100.0f : 0.0f;
        cout << "Energy Source Ratios: Grid: " << gridRatio << "%, Solar: " << solarRatio << "%" << endl;

        int regularBookings = 0, premiumBookings = 0;
        for (int i = 0; i < bookingCount; i++) {
            for (int j = 0; j < userCount; j++) {
                if (users[j].userID == bookings[i].userID) {
                    if (users[j].membershipLevel == 0) regularBookings++;
                    else premiumBookings++;
                    break;
                }
            }
        }
        cout << "User Demand Trends: Regular Bookings: " << regularBookings << ", Premium Bookings: " << premiumBookings << endl;

        float totalRevenue = 0.0f;
        for (int i = 0; i < bookingCount; i++) {
            if (!bookings[i].isActive) totalRevenue += bookings[i].cost;
        }
        cout << "Total Revenue: $" << totalRevenue << endl;

        float co2Savings = 0.0f;
        for (int i = 0; i < bookingCount; i++) {
            if (!bookings[i].isActive) {
                for (int j = 0; j < MAX_DOCKS; j++) {
                    if (docks[j].dockID == bookings[i].dockID && docks[j].energySource != nullptr) {
                        co2Savings += docks[j].energySource->getCO2Emission(bookings[i].energyConsumed);
                        break;
                    }
                }
            }
        }
        cout << "Environmental Impact: CO2 Savings: " << co2Savings << " kg" << endl;

        cout << "=====================================\n";
    }

    void displayDockStatus() {
        cout << "\n=== Charging Station Dock Status ===\n";
        cout << left << setw(10) << "Dock ID" << setw(15) << "Power (kW)" << setw(15) << "Source" << setw(25) << "Status" << endl;
        cout << string(65, '-') << endl;
        for (int i = 0; i < MAX_DOCKS; i++) {
            if (docks[i].energySource == nullptr) {
                cout << "Error: Dock " << docks[i].dockID << " has null energy source!" << endl;
                continue;
            }
            cout << left << setw(10) << docks[i].dockID
                 << setw(15) << docks[i].powerRating
                 << setw(15) << docks[i].energySource->getSourceName();
            if (docks[i].isOccupied) {
                cout << "Occupied (Vehicle ID: " << docks[i].currentVehicleID << ")";
            } else {
                cout << "Available";
            }
            cout << endl;
        }
        cout << "=====================================\n";
    }

    void viewUserBookings(int userID) {
        cout << "\n=== Bookings for User ID: " << userID << " ===\n";
        bool found = false;
        for (int i = 0; i < bookingCount; i++) {
            if (bookings[i].userID == userID) {
                found = true;
                cout << "Booking ID: " << bookings[i].bookingID
                     << ", Vehicle ID: " << bookings[i].vehicleID
                     << ", Dock ID: " << bookings[i].dockID
                     << ", Start Time: " << bookings[i].startTime
                     << ", Duration: " << bookings[i].duration
                     << ", Status: " << (bookings[i].isActive ? "Active" : "Completed") << endl;
            }
        }
        if (!found) {
            cout << "No bookings found for this user." << endl;
        }
    }
};

// Charging Network class
class ChargingNetwork {
public:
    ChargingStation* stations[MAX_STATIONS];

    ChargingNetwork() {
        for (int i = 0; i < MAX_STATIONS; i++) {
            stations[i] = new ChargingStation(i + 1);
        }
    }

    ~ChargingNetwork() {
        for (int i = 0; i < MAX_STATIONS; i++) {
            delete stations[i];
        }
    }

    ChargingStation& getStation(int stationID) {
        if (stationID < 1 || stationID > MAX_STATIONS) {
            cout << "Invalid station ID! Defaulting to Station 1.\n";
            return *stations[0];
        }
        return *stations[stationID - 1];
    }
};

// Main function
int main() {
    ChargingNetwork network;
    int choice, userID, vehicleID, powerRating, membershipLevel, chargingType, stationID, bookingID;
    char name[50];
    float soc, capacity, startTime, duration;
    bool v2g;
    float dischargeEnergy;

    cout << "Welcome to the EV Charging Station System!\n";

    while (true) {
        cout << "\nMenu:\n";
        cout << "1. Register User\n";
        cout << "2. Register Vehicle\n";
        cout << "3. Create Booking\n";
        cout << "4. Complete Booking\n";
        cout << "5. Display Dock Status\n";
        cout << "6. Generate Analytics Report\n";
        cout << "7. Display Real-Time Charging Data\n";
        cout << "8. Cancel Booking\n";
        cout << "9. Discharge to Grid (V2G)\n";
        cout << "10. View User Bookings\n";
        cout << "11. Change Weather Condition\n";
        cout << "12. Exit\n";
        cout << "Enter your choice: ";
        cin >> choice;

        if (choice == 12) break;

        switch (choice) {
            case 1:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter User ID: ";
                cin >> userID;
                cout << "Enter User Name: ";
                cin.ignore();
                cin.getline(name, 50);
                cout << "Enter Membership Level (0 for Regular, 1 for Premium): ";
                cin >> membershipLevel;
                network.getStation(stationID).registerUser(userID, name, membershipLevel);
                break;

            case 2:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter Vehicle ID: ";
                cin >> vehicleID;
                cout << "Enter User ID: ";
                cin >> userID;
                cout << "Enter Battery State of Charge (SOC, 0-100%): ";
                cin >> soc;
                cout << "Enter Battery Capacity (kWh): ";
                cin >> capacity;
                cout << "Supports V2G? (0 for No, 1 for Yes): ";
                cin >> v2g;
                network.getStation(stationID).registerVehicle(vehicleID, userID, soc, capacity, v2g);
                break;

            case 3:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter User ID: ";
                cin >> userID;
                cout << "Enter Vehicle ID: ";
                cin >> vehicleID;
                cout << "Enter Start Time (e.g., 10.0 for 10:00): ";
                cin >> startTime;
                cout << "Enter Duration (hours): ";
                cin >> duration;
                cout << "Enter Desired Charging Speed (1 for Slow - 7 kW, 2 for Medium - 22 kW, 3 for Fast - 50 kW, 4 for Solar - 7 kW): ";
                cin >> chargingType;
                if (chargingType == 1) powerRating = SLOW;
                else if (chargingType == 2) powerRating = MEDIUM;
                else if (chargingType == 3) powerRating = FAST;
                else if (chargingType == 4) powerRating = SOLAR;
                else {
                    cout << "Invalid charging speed!" << endl;
                    break;
                }
                network.getStation(stationID).createBooking(userID, vehicleID, startTime, duration, powerRating, chargingType);
                break;

            case 4:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter Booking ID to complete: ";
                cin >> bookingID;
                network.getStation(stationID).completeBooking(bookingID);
                break;

            case 5:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                network.getStation(stationID).displayDockStatus();
                break;

            case 6:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                network.getStation(stationID).generateReport();
                break;

            case 7:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                network.getStation(stationID).displayRealTimeData();
                break;

            case 8:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter Booking ID to cancel: ";
                cin >> bookingID;
                network.getStation(stationID).cancelBooking(bookingID);
                break;

            case 9:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter Vehicle ID: ";
                cin >> vehicleID;
                cout << "Enter Energy to Discharge (kWh): ";
                cin >> dischargeEnergy;
                {
                    ChargingStation& cs = network.getStation(stationID);
                    bool found = false;
                    for (int i = 0; i < cs.vehicleCount; i++) {
                        if (cs.vehicles[i].vehicleID == vehicleID) {
                            float discharged = cs.vehicles[i].dischargeToGrid(dischargeEnergy);
                            cout << "Discharged " << discharged << " kWh to the grid.\n";
                            found = true;
                            break;
                        }
                    }
                    if(!found) {
                        cout << "Vehicle ID not found.\n";
                    }
                }
                break;

            case 10:
                cout << "Enter Station ID (1-" << MAX_STATIONS << "): ";
                cin >> stationID;
                cout << "Enter User ID: ";
                cin >> userID;
                network.getStation(stationID).viewUserBookings(userID);
                break;

            case 11:
                cout << "Select Weather Condition (0 for Sunny, 1 for Cloudy, 2 for Night): ";
                int weather;
                cin >> weather;
                if (weather == 0) currentWeather = SUNNY;
                else if (weather == 1) currentWeather = CLOUDY;
                else if (weather == 2) currentWeather = NIGHT;
                else {
                    cout << "Invalid weather condition!" << endl;
                    break;
                }
                cout << "Weather condition updated." << endl;
                break;

            default:
                cout << "Invalid choice!" << endl;
        }
    }

    cout << "Thank you for using the EV Charging Station System!" << endl;
    return 0;
}


