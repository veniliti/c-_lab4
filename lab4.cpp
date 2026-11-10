
#include <chrono>
#include <fstream>
#include <random>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
using namespace std;
using namespace std::chrono;

class Data {
    int f[2] = { 0, 0 };
    mutable shared_mutex m[2];

public:
    int read(int i) const
    {
        shared_lock<shared_mutex> lk(m[i]);
        return f[i];
    }
    void write(int i, int v)
    {
        unique_lock<shared_mutex> lk(m[i]);
        f[i] = v;
    }
    operator string() const
    {
        shared_lock<shared_mutex> l0(m[0]);
        shared_lock<shared_mutex> l1(m[1]);
        return to_string(f[0]) + " " + to_string(f[1]);
    }
};


void generate(const string& file, const int w[5], int n, unsigned seed)
{
    mt19937 gen(seed);
    discrete_distribution<int> dist(w, w + 5);
    ofstream out(file);
    for (int i = 0; i < n; ++i) {
        switch (dist(gen)) {
        case 0: out << "read 0\n"; break;
        case 1: out << "write 0 1\n"; break;
        case 2: out << "read 1\n"; break;
        case 3: out << "write 1 1\n"; break;
        default: out << "string\n"; break;
        }
    }
}

void generateAll()
{
    const int N = 6000000;                 
    const int a[5] = { 20, 5, 20, 5, 50 };   
    const int b[5] = { 20, 20, 20, 20, 20 }; 
    const int c[5] = { 2, 60, 3, 33, 2 };    
    generate("test_a.txt", a, N, 1);
    generate("test_b.txt", b, N, 2);
    generate("test_c.txt", c, N, 3);
    cout << "Test files generated\n";
}


struct Op { char type; char field; int value; }; 

vector<Op> load(const string& file)
{
    vector<Op> ops;
    ifstream in(file);
    string cmd;
    while (in >> cmd) {
        Op o{ 0, 0, 0 };
        if (cmd == "read") { o.type = 0; int i; in >> i; o.field = (char)i; }
        else if (cmd == "write") { o.type = 1; int i; in >> i >> o.value; o.field = (char)i; }
        else o.type = 2;
        ops.push_back(o);
    }
    return ops;
}

void run(Data& d, const vector<Op>& ops, size_t from, size_t to, long long& sink)
{
    long long s = 0;
    for (size_t k = from; k < to; ++k) {
        const Op& o = ops[k];
        if (o.type == 0) s += d.read(o.field);
        else if (o.type == 1) d.write(o.field, o.value);
        else s += (long long)static_cast<string>(d).size();
    }
    sink = s;
}

double measure(const vector<Op>& ops, int threads)
{
    Data d;
    vector<long long> sinks(threads);
    vector<thread> th;
    size_t n = ops.size(), chunk = n / threads;
    auto t0 = steady_clock::now();
    for (int t = 0; t < threads; ++t) {
        size_t from = t * chunk, to = (t == threads - 1) ? n : from + chunk;
        th.emplace_back(run, ref(d), cref(ops), from, to, ref(sinks[t]));
    }
    for (auto& x : th) x.join();
    return duration<double>(steady_clock::now() - t0).count();
}

int main()
{
    generateAll();
    const char* names[3] = { "a) as in spec", "b) equal", "c) mismatched" };
    const char* files[3] = { "test_a.txt", "test_b.txt", "test_c.txt" };
    const int RUNS = 3;
    double avg[3][3];
    for (int s = 0; s < 3; ++s) {
        vector<Op> ops = load(files[s]); 
        cout << files[s] << ": " << ops.size() << " operations\n";
        for (int t = 1; t <= 3; ++t) {
            double sum = 0;
            for (int r = 0; r < RUNS; ++r) {
                double x = measure(ops, t);
                cout << "  threads=" << t << " run " << r + 1 << ": " << fixed << setprecision(4) << x << " s\n";
                sum += x;
            }
            avg[s][t - 1] = sum / RUNS;
        }
    }
    cout << "\nAverage time, s (" << RUNS << " runs)\n";
    cout << left << setw(20) << "file" << setw(12) << "1 thread" << setw(12) << "2 threads" << "3 threads\n";
    for (int s = 0; s < 3; ++s)
        cout << left << setw(20) << names[s] << setw(12) << avg[s][0] << setw(12) << avg[s][1] << avg[s][2] << "\n";
}