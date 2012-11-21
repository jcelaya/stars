#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <vector>
#include "../sim/Distributions.hpp"
namespace fs = boost::filesystem;
using namespace std;


double getResolution(const vector<double> & v, double samples) {
    double min = v[0], max = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    return (max - min) / samples;
}


size_t checkAndCountLines(fs::path file) {
    size_t result = 0;
    if (!fs::exists(file)) {
        cout << file.native() << " file not found" << endl;
    } else {
        fs::ifstream ifs(file);
        while (ifs.good()) {
            ifs.ignore(10000, '\n');
            ++result;
        }
    }
    return result;
}


void readFile(fs::path file, double samples, int numFields, int fields[], vector<double> values[]) {
    string line;
    size_t numLines;
    if ((numLines = checkAndCountLines(file))) {
        for (int i = 0; i < numFields; ++i)
            values[i].reserve(numLines);

        fs::ifstream ifs(file);
        while (ifs.good()) {
            getline(ifs, line);
            // Return on blank line
            if (line.length() == 0) break;
            // Jump comments
            if (line[0] == '#') continue;
            // Else process line
            int findex = 0;
            // Select first field
            size_t firstpos = 0, lastpos = line.find_first_of(',');
            for (int field = 0; findex < numFields; ++field) {
                if (field == fields[findex]) {
                    // Get the value
                    values[findex].push_back(0.0);
                    istringstream(line.substr(firstpos, lastpos - firstpos)) >> values[findex].back();
                    ++findex;
                }
                firstpos = lastpos + 1;
                lastpos = line.find_first_of(',', firstpos);
            }
        }
    }
}


void getHistograms(double samples, int numHists, const vector<double> values[], Histogram hists[]) {
    for (int i = 0; i < numHists; ++i) {
        hists[i] = Histogram(getResolution(values[i], samples));
        for (size_t j = 0; j < values[i].size(); ++j)
            hists[i].addValue(values[i][j]);
    }
}


void readAppsFile(fs::path resultDir, double samples) {
    // We use fields 2, 3, 8, 9, 10 and 11
    int fields[6] = { 2, 3, 8, 9, 10, 11 };
    // Reserve an extra vector for speedup values,
    vector<double> values[7];
    readFile(resultDir / fs::path("apps.stat"), samples, 6, fields, values);
    if (!values[0].empty()) {
        double totalAcceptedTasks = 0.0;
        double totalComputation = 0.0;

        for (size_t j = 0; j < values[1].size(); ++j) {
            totalAcceptedTasks += values[2][j];
            totalComputation += values[1][j] * values[2][j];
        }

        // Calculate accepted and rejected task length histograms first
        Histogram acceptedLength(getResolution(values[1], samples));
        for (size_t j = 0; j < values[1].size(); ++j)
            for (size_t i = 0; i < values[2][j]; ++i)
                acceptedLength.addValue(values[1][j]);
        Histogram rejectedLength(getResolution(values[1], samples));
        for (size_t j = 0; j < values[1].size(); ++j)
            for (size_t i = 0; i < (values[0][j] - values[2][j]); ++i)
                rejectedLength.addValue(values[1][j]);

        // Calculate speedup
        values[6].resize(values[0].size());
        for (size_t i = 0; i < values[0].size(); ++i)
            values[6][i] = values[4][i] * values[2][i] / values[0][i] / values[3][i];
        // Calculate % of finished tasks
        for (size_t i = 0; i < values[0].size(); ++i)
            values[2][i] *= 100.0 / values[0][i];

        Histogram hists[7];
        getHistograms(samples, 7, values, hists);

        fs::ofstream ofs(resultDir / "apps_cdf.stat");
        ofs.precision(8);
        ofs << "# accepted tasks, total computation, average task length" << std::endl;
        ofs << "# " << totalAcceptedTasks << ' ' << totalComputation << ' ' << (totalComputation / totalAcceptedTasks) << std::endl;
        ofs << "# Finished % CDF" << std::endl << CDF(hists[2]) << std::endl << std::endl;
        ofs << "# Accepted task lengths CDF" << std::endl << CDF(acceptedLength) << std::endl << std::endl;
        ofs << "# Rejected task lengths CDF" << std::endl << CDF(rejectedLength) << std::endl << std::endl;
        ofs << "# JTT CDF" << std::endl << CDF(hists[3]) << std::endl << std::endl;
        ofs << "# Sequential time in src CDF" << std::endl << CDF(hists[4]) << std::endl << std::endl;
        ofs << "# Speedup CDF" << std::endl << CDF(hists[6]) << std::endl << std::endl;
        ofs << "# Slowness CDF" << std::endl << CDF(hists[5]);
    }
}


void readRequestsFile(fs::path resultDir, double samples) {
    // We use fields 4 and 7
    int fields[2] = { 4, 7 };
    vector<double> values[2];
    readFile(resultDir / fs::path("requests.stat"), samples, 2, fields, values);
    if (!values[0].empty()) {
        Histogram hists[2];
        getHistograms(samples, 2, values, hists);

        fs::ofstream ofs(resultDir / "requests_cdf.stat");
        ofs.precision(8);
        ofs << "# Number of nodes CDF" << std::endl << CDF(hists[0]) << std::endl << std::endl;
        ofs << "# Search time CDF" << std::endl << CDF(hists[1]);
    }
}


void readCPUFile(fs::path resultDir, double samples) {
    // We use fields 1
    int fields[1] = { 1 };
    vector<double> values[1];
    readFile(resultDir / fs::path("cpu.stat"), samples, 1, fields, values);
    if (!values[0].empty()) {
        Histogram hists[1];
        getHistograms(samples, 1, values, hists);

        fs::ofstream ofs(resultDir / "cpu_cdf.stat");
        ofs << "# CDF of num of executed tasks" << endl << CDF(hists[0]);
    }
}


void readAvailFile(fs::path resultDir, double samples) {
    // We use fields 0 and 1
    int fields[2] = { 0, 1 };
    vector<double> values[2];
    readFile(resultDir / fs::path("availability.stat"), samples, 2, fields, values);
    if (!values[0].empty()) {
        Histogram hists[2];
        getHistograms(samples, 2, values, hists);

        fs::ofstream ofs(resultDir / "availability_cdf.stat");
        ofs.precision(8);
        ofs << "# Update time CDF" << std::endl << CDF(hists[0]) << std::endl << std::endl;
        ofs << "# Reached level CDF" << std::endl << CDF(hists[1]);
    }
}


void readTrafficFile(fs::path resultDir, double samples) {
//    // We use fields 0 and 1
//    int fields[2] = { 0, 1 };
//    vector<double> values[2];
//    readFile(resultDir / fs::path("traffic.stat"), samples, 2, fields, values);
//    if (!values[0].empty()) {
//        Histogram hists[2];
//        getHistograms(samples, 2, values, hists);
//
//        fs::ofstream ofs(resultDir / "traffic_cdf.stat");
//        ofs.precision(8);
//        ofs << "# Update time CDF" << std::endl << CDF(hists[0]) << std::endl << std::endl;
//        ofs << "# Reached level CDF" << std::endl << CDF(hists[1]);
//    }
}


int main(int argc, char * argv[]) {
    // Get number of samples per histogram
    double samples = 1000.0;
    bool dirsInCmndLine = false;
    fs::path resultDir;

    // Parse command line
    for (int i = 1; i < argc; i++) {
        if (argv[i] == string("-s") && ++i < argc)
            istringstream(argv[i]) >> samples;
        else {
            dirsInCmndLine = true;
            resultDir = argv[i];
            if (!fs::exists(resultDir)) {
                cout << "Directory " << resultDir << " does not exist." << endl;
                continue;
            }

            cout << "Creating statistics in " << resultDir << endl;

            readAppsFile(resultDir, samples);
            readRequestsFile(resultDir, samples);
            readCPUFile(resultDir, samples);
            readAvailFile(resultDir, samples);
            readTrafficFile(resultDir, samples);
        }
    }

    if (!dirsInCmndLine) {
        // Examine "./"
        resultDir = ".";

        cout << "Creating statistics in " << resultDir << endl;

        readAppsFile(resultDir, samples);
        readRequestsFile(resultDir, samples);
        readCPUFile(resultDir, samples);
        readAvailFile(resultDir, samples);
        readTrafficFile(resultDir, samples);
    }

    return 0;
}

