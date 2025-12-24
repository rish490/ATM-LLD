/*
====================================================================
ATM System LLD in C++ - Interface-Based Design
====================================================================

This implementation models a simple ATM system with the following
key entities:

1. ATM (Interface Layer)
   - Represents the physical ATM machine (or a client).
   - Handles user input, menu display, and interaction with the bank.
   - Does NOT contain business logic; it delegates all operations
     to BankService.

2. BankService (Interface: IBankService)
   - Provides an abstract interface for ATM to interact with the
     bank system.
   - All business logic such as deposit, withdrawal, balance check,
     and transaction history is handled by the bank service.
   - It is intentionally an interface to decouple ATM from any
     concrete backend implementation:
       * ATM only knows the contract (methods available),
         not the internal workings.
       * Supports multiple backend implementations: real bank API,
         mock service for testing, or third-party systems.
       * Facilitates testability, dependency injection, and
         future extensibility.

3. User
   - Represents a customer of the bank.
   - Authenticates via PIN.
   - Can have multiple accounts.

4. Account
   - Represents a bank account with balance and transaction history.
   - Contains the actual logic for deposit, withdrawal, and transaction
     recording.
   - Not an interface because the ATM does NOT interact with accounts
     directly; it communicates through the BankService.
   - Interface abstraction is unnecessary unless ATM needs to handle
     multiple account types differently.

5. Transaction
   - Represents individual deposits or withdrawals.
   - Stores type, amount, and timestamp.

----------------------------
Design Rationale
----------------------------
- Separation of Concerns:
  * ATM = interface for users.
  * BankService = business logic.
  * Account = domain entity with balance and transactions.
- Decoupling via Interface:
  * ATM depends only on IBankService, not concrete implementation.
- Extensible & Testable:
  * New bank implementations or mock services can be added without
    changing ATM code.
- Realistic System Modeling:
  * Mimics how ATMs communicate with a bank backend in real systems.
  * ATM never directly manipulates account data; all operations go
    through the BankService interface.

====================================================================
*/




#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>

using namespace std;

// ---------------- Transaction ----------------
enum class TransactionType { DEPOSIT, WITHDRAW };

class Transaction {
private:
    TransactionType type;
    double amount;
    string timestamp;

    string getCurrentTime() {
        time_t now = time(0);
        char* dt = ctime(&now);
        return string(dt);
    }

public:
    Transaction(TransactionType t, double amt) : type(t), amount(amt) {
        timestamp = getCurrentTime();
    }

    void show() const {
        string tType = (type == TransactionType::DEPOSIT) ? "Deposit" : "Withdraw";
        cout << timestamp << " | " << tType << " | Amount: $" << amount << endl;
    }
};

// ---------------- Account ----------------
class Account {
private:
    string accountNumber;
    double balance;
    vector<Transaction> transactions;
    mutable mutex mtx; // Pessimistic lock for thread safety

public:
    Account(string accNum, double bal = 0) : accountNumber(accNum), balance(bal) {}

    string getAccountNumber() const { return accountNumber; }

    // Deposit with thread safety
    void deposit(double amount) {
        lock_guard<mutex> lock(mtx);  // Pessimistic lock: lock for entire operation
        balance += amount;
        transactions.push_back(Transaction(TransactionType::DEPOSIT, amount));
        cout << "Deposit successful! Balance: $" << balance << endl;
    }

    // Withdraw with thread safety
    bool withdraw(double amount) {
        lock_guard<mutex> lock(mtx);  // Lock ensures no other operation can modify balance simultaneously
        if (amount > balance) {
            cout << "Insufficient funds!" << endl;
            return false;
        }
        balance -= amount;
        transactions.push_back(Transaction(TransactionType::WITHDRAW, amount));
        cout << "Withdrawal successful! Balance: $" << balance << endl;
        return true;
    }

    double getBalance() const {
        lock_guard<mutex> lock(mtx); // Lock ensures reading correct balance
        return balance;
    }

    void showTransactions() const {
        lock_guard<mutex> lock(mtx); // Lock ensures consistent transaction history
        if (transactions.empty()) {
            cout << "No transactions yet." << endl;
            return;
        }
        cout << "Transaction history for account " << accountNumber << ":\n";
        for (const auto& t : transactions) {
            t.show();
        }
    }
};

// ---------------- User ----------------
class User {
private:
    string name;
    string pin;
    vector<Account*> accounts;

public:
    User(string n, string p) : name(n), pin(p) {}

    bool authenticate(string inputPin) const { return pin == inputPin; }

    void addAccount(Account* account) { accounts.push_back(account); }

    vector<Account*>& getAccounts() { return accounts; }
};

// ---------------- Bank Service Interface ----------------
class IBankService {
public:
    virtual bool deposit(const string& accNum, double amount) = 0;
    virtual bool withdraw(const string& accNum, double amount) = 0;
    virtual double getBalance(const string& accNum) = 0;
    virtual void showTransactions(const string& accNum) = 0;
    virtual User* getUserByAccount(const string& accNum) = 0;
    virtual Account* getAccount(const string& accNum) = 0;
    virtual ~IBankService() {}
};

// ---------------- Concrete BankService ----------------
class BankService : public IBankService {
private:
    unordered_map<string, User*> users;
    unordered_map<string, Account*> accounts;

public:
    void addUser(User* user) {
        for (auto acc : user->getAccounts()) {
            users[acc->getAccountNumber()] = user;
            accounts[acc->getAccountNumber()] = acc;
        }
    }

    bool deposit(const string& accNum, double amount) override {
        Account* acc = getAccount(accNum);
        if (!acc) return false;
        acc->deposit(amount);
        return true;
    }

    bool withdraw(const string& accNum, double amount) override {
        Account* acc = getAccount(accNum);
        if (!acc) return false;
        return acc->withdraw(amount);
    }

    double getBalance(const string& accNum) override {
        Account* acc = getAccount(accNum);
        if (!acc) return -1;
        return acc->getBalance();
    }

    void showTransactions(const string& accNum) override {
        Account* acc = getAccount(accNum);
        if (acc) acc->showTransactions();
    }

    User* getUserByAccount(const string& accNum) override {
        if (users.find(accNum) != users.end())
            return users[accNum];
        return nullptr;
    }

    Account* getAccount(const string& accNum) override {
        if (accounts.find(accNum) != accounts.end())
            return accounts[accNum];
        return nullptr;
    }
};

// ---------------- ATM (Interface Layer) ----------------
class ATM {
private:
    IBankService* bankService;
    User* currentUser = nullptr;
    Account* currentAccount = nullptr;

public:
    ATM(IBankService* service) : bankService(service) {}

    bool login(const string& accNum, const string& pin) {
        User* user = bankService->getUserByAccount(accNum);
        if (user && user->authenticate(pin)) {
            currentUser = user;
            currentAccount = bankService->getAccount(accNum);
            cout << "Login successful!\n";
            return true;
        }
        cout << "Invalid account number or PIN.\n";
        return false;
    }

    void logout() {
        currentUser = nullptr;
        currentAccount = nullptr;
        cout << "Logged out successfully.\n";
    }

    void showMenu() {
        if (!currentUser) {
            cout << "Please login first.\n";
            return;
        }

        int choice;
        double amount;
        do {
            cout << "\n--- ATM Menu ---\n";
            cout << "1. Check Balance\n";
            cout << "2. Deposit\n";
            cout << "3. Withdraw\n";
            cout << "4. Show Transactions\n";
            cout << "5. Logout\n";
            cout << "Enter choice: ";
            cin >> choice;

            switch (choice) {
                case 1:
                    cout << "Balance: $" << bankService->getBalance(currentAccount->getAccountNumber()) << endl;
                    break;
                case 2:
                    cout << "Enter amount to deposit: ";
                    cin >> amount;
                    bankService->deposit(currentAccount->getAccountNumber(), amount);
                    break;
                case 3:
                    cout << "Enter amount to withdraw: ";
                    cin >> amount;
                    bankService->withdraw(currentAccount->getAccountNumber(), amount);
                    break;
                case 4:
                    bankService->showTransactions(currentAccount->getAccountNumber());
                    break;
                case 5:
                    logout();
                    break;
                default:
                    cout << "Invalid choice.\n";
            }
        } while (choice != 5 && currentUser);
    }
};

// ---------------- Main ----------------
int main() {
    BankService bank;

    // Create users and accounts
    User* user1 = new User("Alice", "1234");
    Account* acc1 = new Account("ACC1001", 1000);
    user1->addAccount(acc1);

    User* user2 = new User("Bob", "4321");
    Account* acc2 = new Account("ACC2001", 500);
    user2->addAccount(acc2);

    bank.addUser(user1);
    bank.addUser(user2);

    ATM atm(&bank);

    string accNum, pin;
    cout << "Enter account number: ";
    cin >> accNum;
    cout << "Enter PIN: ";
    cin >> pin;

    if (atm.login(accNum, pin)) {
        atm.showMenu();
    }

    return 0;
}
