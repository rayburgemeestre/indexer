/*
  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// g++ --std=c++14 % -o a.out && ./a.out
// export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH; g++ --std=c++14 % -o a.out && ./a.out
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <thread>
//#include <execution>

#include "crow.h"

#pragma once

class indexer;
class webserver
{
public:
    template <typename T>
    webserver(indexer &indexer_, T start_webserver)
      : indexer_(indexer_), webserver_(start_webserver) {}

    ~webserver()
    {
        webserver_.join();
    }

    std::thread webserver_;
    indexer &indexer_;
};

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
    mutable vector<const node *> children_;
    mutable size_t my_hash_;
    mutable size_t cum_kilobyte_;

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
    vector<const node *> & children() const { return children_; }
    const size_t &my_hash() const { return my_hash_; }
    void set_hash(size_t hash) const { my_hash_ = hash; };
    const size_t &cum_kilobyte() const { return cum_kilobyte_; }
    void set_cum_kilobyte(size_t kb) const { cum_kilobyte_ = kb; };
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

hash<string> hash_fn;

void recurse(set<size_t> &hashes, unordered_multimap<size_t, const node *> &hash_to_node, const node &n) {
    size_t my_hash = hash_fn(n.basename() + to_string(n.kilobyte()) + n.filetype());
    size_t my_cum_kb = n.kilobyte();
    for (const node *child : n.children()) {
        recurse(hashes, hash_to_node, *child);
        my_hash = my_hash ^ child->my_hash();
        my_cum_kb += child->kilobyte();
    }
    n.set_hash(my_hash);
    n.set_cum_kilobyte(my_cum_kb);
    hash_to_node.insert({my_hash, &n});
    hashes.insert(my_hash);
    return;
}

#include "md5.h"

class indexer
{
public: // too lazy right now to make getters
    vector<node> nodes;
    unordered_multimap<string, size_t> basenames;
    unordered_multimap<string, size_t> fullnames;
    unordered_multimap<size_t, const node *> hash_to_node;
    set<size_t> hashes;
    vector<pair<size_t, const node *>> nodes_by_size;

    std::string filename_;
    node root;
public:
    indexer(std::string filename);

    void run();

private:
    void read_nodes_and_sort();
    void create_lookup_tables_and_sort();
    void create_tree_structure();
    void create_hashes_on_tree();
};

indexer::indexer(std::string filename) : filename_(filename), root("root") {

}

void indexer::run() {
    read_nodes_and_sort();
    create_lookup_tables_and_sort();
    create_tree_structure();
    create_hashes_on_tree();
}

void indexer::read_nodes_and_sort() {
    cout << "reading index file... ";
    timer s;
    ifstream input(filename_);
    size_t counter = 0;
    for (string line; getline(input, line);) {
        nodes.emplace_back(line);
        counter++;
        //if (counter == 1000) break; // just for testing
    }
    input.close();
    cout << "lines read: " << counter << endl;
    cout << "elapsed seconds: " << s.stop() << endl;
    cout << "sorting files in memory..\n";
    timer s2;
    //sort(execution::par, nodes.begin(), nodes.end());
    sort(nodes.begin(), nodes.end());
    cout << "elapsed seconds: " << s2.stop() << endl;
}

void indexer::create_lookup_tables_and_sort()
{
    cout << "creating lookup tables..\n";
    timer s3;
    size_t counter = 0;
    basenames.reserve(nodes.size());
    fullnames.reserve(nodes.size());
    nodes_by_size.reserve(nodes.size());
    for (const auto &node : nodes) {
        basenames.insert({node.basename(), counter});
        counter++;
    }
    for (const auto &node : nodes) {
        nodes_by_size.push_back(make_pair(node.kilobyte(), &node));
    }
    counter = 0;
    for (const auto &node : nodes) {
        fullnames.insert({node.file(), counter});
        counter++;
    }
    /*
    node root("root");
    for (const auto &node : nodes) {
        // hash input: cout << "node = " << node.kilobyte() << node.file() << node.filetype() << endl;
        auto &me = node;
        auto parent = node.file().substr(0, node.file().length() - node.basename().length() - 1);
        auto iter = fullnames.find(parent);
        if (iter == fullnames.end()) {
            cout << "Found root node: " << parent << endl;
            root.children().push_back(&node);
        } else {
            const auto &myparent = nodes[iter->second];
            myparent.children().push_back(&node);
        }
        counter++;
    }
    cout << "root got childs: " << root.children().size() << endl;
     */
    cout << "elapsed seconds: " << s3.stop() << endl;


    cout << "sorting lookup tables..\n";
    timer s4;
    std::sort(nodes_by_size.begin(), nodes_by_size.end(), [](const auto &p1, const auto &p2) {
        if (p1.first == p2.first) {
           return p1.second->basename() < p2.second->basename();
        }
        return p1.first > p2.first;
    });
    cout << "elapsed seconds: " << s4.stop() << endl;
}

void indexer::create_tree_structure()
{
    cout << "creating tree structure..\n";
    timer s3;
    size_t counter = 0;
    for (const auto &node : nodes) {
        // hash input: cout << "node = " << node.kilobyte() << node.file() << node.filetype() << endl;
        auto &me = node;
        auto parent = node.file().substr(0, node.file().length() - node.basename().length() - 1);
        auto iter = fullnames.find(parent);
        if (iter == fullnames.end()) {
            cout << "Found root node: " << parent << endl;
            root.children().push_back(&node);
        } else {
            const auto &myparent = nodes[iter->second];
            myparent.children().push_back(&node);
        }
        counter++;
    }
    cout << "root got childs: " << root.children().size() << endl;
    cout << "elapsed seconds: " << s3.stop() << endl;
}

void indexer::create_hashes_on_tree()
{
    cout << "creating hashes recursively..\n";
    timer s5;
    recurse(hashes, hash_to_node, root);
    cout << "elapsed seconds: " << s5.stop() << endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cerr << "Usage " << argv[0] << " <index>" << endl;
    }

    indexer indexer_(argv[1]);
    indexer_.run();

    cout << "listing all duplicate folders > 1GiB..\n";
    timer s6;
    for (const size_t &hash : indexer_.hashes) {
        const auto c = indexer_.hash_to_node.count(hash);
        if (c < 2) {
            continue;
        }
        auto range = indexer_.hash_to_node.equal_range(hash);
        if (range.first->second->filetype() == 'd' && range.first->second->cum_kilobyte() >= (1024 * 1024 /* 1 GiB */)) {
            cout << "hash " << hash << " occurs " << c << " times..." << endl;
            for_each(range.first, range.second, [](auto &p) {
                cout << " to be specific: " << p.second->file() << endl;
            });
        }
    }
    cout << "elapsed seconds: " << s6.stop() << endl;

    webserver ws(indexer_, [&]() -> void {
        crow::App<> app;

        CROW_ROUTE(app, "/")
        ([&]{
            std::ifstream ifile("index.html", ios::binary);
            std::string s( (std::istreambuf_iterator<char>(ifile)),
                           (std::istreambuf_iterator<char>()) );
            return s;
        });

        CROW_ROUTE(app, "/find")
            .methods("POST"_method)
        ([&](const crow::request &req) {
            ostringstream ss;
            auto iter = indexer_.basenames.find(req.body);
            if (iter == indexer_.basenames.end()) {
                ss << "Nothing found. Try using 'match'...\n";
            } else {
                vector<string> results;
                for (; iter!=indexer_.basenames.begin(); iter++) {
                    const auto &node = indexer_.nodes[iter->second];
                    if (node.basename() != req.body) {
                        break;
                    }
                    results.emplace_back(node.file());
                }
                sort(results.begin(), results.end());
                for (const auto &result : results) {
                    ss << result << endl;
                }
            }
            return crow::response{ss.str()};
        });

        CROW_ROUTE(app, "/match")
            .methods("POST"_method)
        ([&](const crow::request &req) {
            ostringstream ss;
            bool only_folders = false;//command == "matchdir";
            size_t counter = 0;
            ss << "matching " << req.body << endl;
            for (const auto &node : indexer_.nodes) {
                if (node.basename().find(req.body) != string::npos) {
                    if (only_folders && node.filetype() != 'd')
                        continue;
                    ss << node.file() << endl;
                    counter++;
                    if (counter >= 100) {
                        ss << "Enough matches, cancelling..\n";
                        break;
                    }
                }
            }
            return crow::response{ss.str()};
        });

        CROW_ROUTE(app, "/dupes")
            .methods("POST"_method)
        ([&](const crow::request &req) {
            ostringstream ss;
            bool only_folders = true;
            size_t counter = 0;
            timer s6;
            for (const size_t &hash : indexer_.hashes) {
                const auto c = indexer_.hash_to_node.count(hash);
                if (c < 2) {
                    continue;
                }
                auto range = indexer_.hash_to_node.equal_range(hash);
                if (range.first->second->filetype() == 'd' && range.first->second->cum_kilobyte() >= (std::stoi(req.body)) * 1024) {
                    ss << "hash " << hash << " occurs " << c << " times..." << endl;
                    for_each(range.first, range.second, [&ss, &range](auto &p) {
                        ss << "  - " << p.second->file() << " (" << (range.first->second->cum_kilobyte() / 1024) << " MiB)" << endl;
                    });
                    counter++;
                    if (counter >= 100) {
                        ss << "Enough matches, cancelling..\n";
                        break;
                    }
                }
            }
            cout << "listing all duplicate folders > 1GiB..\n";
            cout << "elapsed seconds: " << s6.stop() << endl;
            return crow::response{ss.str()};
        });

        CROW_ROUTE(app, "/by_size")
            .methods("POST"_method)
        ([&](const crow::request &req) {
            ostringstream ss;
            size_t counter = 0;
            timer s6;
            for (const auto &p : indexer_.nodes_by_size) {
                const auto & KiB = p.first;
                const auto & node = *p.second;
                ss << "match: " << (node.kilobyte() / 1024) << "MiB " << node.file() << endl;
                counter++;
                if (counter >= 100) {
                    cout << "Enough matches, cancelling..\n";
                    break;
                }
            }
            cout << "elapsed seconds: " << s6.stop() << endl;
            return crow::response{ss.str()};
        });

        //crow::logger::setLogLevel(crow::LogLevel::DEBUG);

        app.port(8888)
            .multithreaded()
            .run();
    });


#if 1 == 2


//    cout << "Type folder name:" << endl;
//    MD5 md5;
//    for (string line; getline(cin, line);) {
//
//        if (line.find(" ") == string::npos) {
//            for (size_t i=0; i< 256; i++) { cout << "\n"; }
//            cout << "Usage: <command> <param>\n";
//            cout << "  i.e. find foo (find basename exactly matching foo)\n";
//            cout << "  i.e. match foo (find basename containing foo)\n";
//            cout << "  i.e. matchdir foo (find folders containing foo)\n";
//            cout << "  i.e. by size (display largest file(s) ordered by size DESC))\n";
//            cout << "Type folder name:" << endl;
//            continue;
//        }
//        auto command = line.substr(0, line.find(" "));
//        line = line.substr(line.find(" ") + 1);
//
//        if (command == "find") {
//            auto iter = basenames.find(line);
//            if (iter == basenames.end()) {
//                cout << "Nothing found. Try using 'match'...\n";
//            } else {
//                vector<string> results;
//                for (; iter!=basenames.begin(); iter++) {
//                    const auto &node = nodes[iter->second];
//                    if (node.basename() != line) {
//                        break;
//                    }
//                    results.emplace_back(node.file());
//                }
//                sort(results.begin(), results.end());
//                for (const auto &result : results) {
//                    cout << result << endl;
//                }
//            }
//        }
//        else if (command == "match" || command == "matchdir") {
//            bool only_folders = command == "matchdir";
//            size_t counter = 0;
//            cout << "matching " << line << endl;
//            for (const auto &node : nodes) {
//                if (node.basename().find(line) != string::npos) {
//                    if (only_folders && node.filetype() != 'd')
//                        continue;
//                    cout << "match: " << node.file() << endl;
//                    counter++;
//                    if (counter >= 100) {
//                        cout << "Enough matches, cancelling..\n";
//                        break;
//                    }
//                }
//            }
//        }
//        else if (command == "by" /* by size ;'-) */) {
//            size_t counter = 0;
//            cout << "matching " << line << endl;
//            for (const auto &p : nodes_by_size) {
//                const auto & KiB = p.first;
//                const auto & node = *p.second;
//                cout << "match: " << (node.kilobyte() / 1024) << "MiB " << node.file() << endl;
//                counter++;
//                if (counter >= 100) {
//                    cout << "Enough matches, cancelling..\n";
//                    break;
//                }
//            }
//        }
//        else if (command == "calc" /* md5 */) {
//            for (const auto &node : nodes) {
//                if (node.filetype() != 'd') {
//                    cout << md5.digestFile( (char *)node.file().c_str() )  << endl;
//                }
//            }
//        }
//        cout << "Type folder name:" << endl;
//    }

#endif

    cout << "Freeing memory..\n";
    return 0;
}

