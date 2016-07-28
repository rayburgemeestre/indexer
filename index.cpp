// g++ --std=c++14 % -o a.out && ./a.out
// export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH; g++ --std=c++14 % -o a.out && ./a.out
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <chrono>
#include <cstdlib>
//#include <execution>
using namespace std;

class node
{
private:
    mutable string file_;
    mutable string basename_;
    mutable size_t kilobyte_;
    mutable uint64_t inode_;
    mutable char filetype_;
    mutable string date_;

public:
    node(const string &file) : file_(file)
    {
        size_t poslast = file_.find_last_of("/");
        if (poslast != string::npos) {
            basename_.assign(file.substr(1 + poslast));
        }

        // regex was too slow
        size_t pos = 0;
        size_t pos2 = file_.find("\t");
        if (pos != string::npos) {
            kilobyte_ = atoll(file.substr(pos, pos2).c_str());
        }
        pos += pos2 + 1;
        pos2 = file_.substr(pos).find("\t");
        if (pos != string::npos) {
            inode_ = atoll(file.substr(pos, pos2).c_str());
        }
        pos += pos2 + 1;
        pos2 = file_.substr(pos).find("\t");
        if (pos != string::npos) {
            date_ = file.substr(pos, pos2);
        }
        pos += pos2 + 1;
        pos2 = file_.substr(pos).find("\t");
        if (pos != string::npos) {
            filetype_ = file.substr(pos, 1)[0];
        }

        file_ = file.substr(pos + pos2 + 1);
        
    }
    const size_t &kilobyte() const { return kilobyte_; }
    const uint64_t &inode() const { return inode_; }
    const string &file() const { return file_; }
    const string &basename() const { return basename_; }
    const char &filetype() const { return filetype_; }
    const string &date() const { return date_; }
};

inline bool operator<(const node &lhs, const node &rhs) {
    return lhs.basename() < rhs.basename();
}

class timer
{
private:
    chrono::time_point<chrono::system_clock> start_, end_;

public:
    timer()
        : start_(chrono::system_clock::now()),
          end_(chrono::system_clock::now())
    {}
    double stop() {
        end_ = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = end_ - start_;

        return elapsed_seconds.count();
    }
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cerr << "Usage " << argv[0] << " <index>" << endl;
    }

    vector<node> nodes;
    unordered_multimap<string, size_t> basenames;

    cout << "reading index file... ";
    timer s;
    ifstream input(argv[1]);
    size_t counter = 0;
    for (string line; getline(input, line);) {
        nodes.emplace_back(line);
        counter++;
        if (counter == 10000) break; // just for testing
    }
    input.close();
    cout << "lines read: " << counter << endl;
    cout << "elapsed seconds: " << s.stop() << endl;

    cout << "sorting files in memory..\n";
    timer s2;
    //sort(execution::par, nodes.begin(), nodes.end());
    sort(nodes.begin(), nodes.end());
    cout << "elapsed seconds: " << s2.stop() << endl;
    
    cout << "creating lookup tables..\n";
    timer s3;
    counter = 0;
    for (const auto &node : nodes) {
        basenames.insert({node.basename(), counter});
        counter++;
    }
    cout << "elapsed seconds: " << s3.stop() << endl;

    cout << "Type folder name:" << endl;
    for (string line; getline(cin, line);) {

        if (line.find(" ") == string::npos) {
            for (size_t i=0; i< 256; i++) { cout << "\n"; }
            cout << "Usage: <command> <param>\n";
            cout << "  i.e. find foo (find basename exactly matching foo)\n";
            cout << "  i.e. match foo (find basename containing foo)\n";
            cout << "  i.e. matchdir foo (find folders containing foo)\n";
            cout << "Type folder name:" << endl;
            continue;
        }
        auto command = line.substr(0, line.find(" "));
        line = line.substr(line.find(" ") + 1);

        if (command == "find") {
            auto iter = basenames.find(line);
            if (iter == basenames.end()) {
                cout << "Nothing found. Try using 'match'...\n";
            } else {
                vector<string> results;
                for (; iter!=basenames.begin(); iter++) {
                    const auto &node = nodes[iter->second];
                    if (node.basename() != line) {
                        break;
                    }
                    results.emplace_back(node.file());
                }
                sort(results.begin(), results.end());
                for (const auto &result : results) {
                    cout << result << endl;
                }
            }
        }
        else if (command == "match" || command == "matchdir") {
            bool only_folders = command == "matchdir";
            size_t counter = 0;
            cout << "matching " << line << endl;
            for (const auto &node : nodes) {
                if (node.basename().find(line) != string::npos) {
                    if (only_folders && node.filetype() != 'd')
                        continue;
                    cout << "match: " << node.file() << endl;
                    counter++;
                    if (counter >= 100) {
                        cout << "Enough matches, cancelling..\n";
                        break;
                    }
                }
            }
        }
        cout << "Type folder name:" << endl;
    }

    cout << "Freeing memory..\n";
    return 0;
}

