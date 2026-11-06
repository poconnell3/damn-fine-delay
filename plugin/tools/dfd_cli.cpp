// Offline renderer for null tests against the Python reference.
//   dfd_cli ir <fs> <oversampling> <A|B> <widener|dual|cascade> <echo 0|1> <seconds> <out.f64>
//     Writes left then right (float64) impulse responses, aligned so sample 0 = the impulse.
//   dfd_cli wav <fs> <oversampling> <A|B> <widener|dual|cascade> <driveDb> <mix> <in.f64> <out.f64>
//     Processes a mono float64 file through the full engine (nonlinear, noise, crosstalk on).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Engine.h"

using namespace dfd;

namespace
{
Routing parseRouting (const char* s)
{
    if (std::strcmp (s, "dual") == 0)
        return Routing::Dual;
    if (std::strcmp (s, "cascade") == 0)
        return Routing::Cascade;
    return Routing::Widener;
}

std::vector<double> readFile (const char* path)
{
    std::vector<double> v;
    if (FILE* f = std::fopen (path, "rb"))
    {
        double x;
        while (std::fread (&x, sizeof x, 1, f) == 1)
            v.push_back (x);
        std::fclose (f);
    }
    return v;
}

bool writeFile (const char* path, const std::vector<double>& a, const std::vector<double>& b)
{
    FILE* f = std::fopen (path, "wb");
    if (f == nullptr)
        return false;
    std::fwrite (a.data(), sizeof (double), a.size(), f);
    std::fwrite (b.data(), sizeof (double), b.size(), f);
    std::fclose (f);
    return true;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc >= 9 && std::strcmp (argv[1], "ir") == 0)
    {
        const double fs = std::atof (argv[2]);
        EngineSetup setup;
        setup.oversampling = std::atoi (argv[3]);
        setup.echo = std::atoi (argv[6]) != 0;
        EngineParams p;
        p.color = argv[4][0] == 'A' ? Color::A : Color::B;
        p.routing = parseRouting (argv[5]);
        p.nonlinear = false;
        p.noise = NoiseMode::Off;
        p.crosstalk = false;
        Engine e;
        e.prepare (fs, setup, p);
        const int n = (int) (std::atof (argv[7]) * fs);
        const int lat = e.latencySamples();
        std::vector<double> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n + lat; ++i)
        {
            double ol, orr;
            e.processSample (i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0, ol, orr);
            if (i >= lat)
            {
                l[(size_t) (i - lat)] = ol;
                r[(size_t) (i - lat)] = orr;
            }
        }
        return writeFile (argv[8], l, r) ? 0 : 1;
    }
    if (argc >= 10 && std::strcmp (argv[1], "wav") == 0)
    {
        const double fs = std::atof (argv[2]);
        EngineSetup setup;
        setup.oversampling = std::atoi (argv[3]);
        EngineParams p;
        p.color = argv[4][0] == 'A' ? Color::A : Color::B;
        p.routing = parseRouting (argv[5]);
        p.driveDb = std::atof (argv[6]);
        p.mix = std::atof (argv[7]);
        Engine e;
        e.prepare (fs, setup, p);
        const auto in = readFile (argv[8]);
        const int lat = e.latencySamples();
        std::vector<double> l (in.size()), r (in.size());
        for (size_t i = 0; i < in.size() + (size_t) lat; ++i)
        {
            const double x = i < in.size() ? in[i] : 0.0;
            double ol, orr;
            e.processSample (x, x, ol, orr);
            if (i >= (size_t) lat)
            {
                l[i - (size_t) lat] = ol;
                r[i - (size_t) lat] = orr;
            }
        }
        return writeFile (argv[9], l, r) ? 0 : 1;
    }
    std::fprintf (stderr, "usage: see the comment at the top of tools/dfd_cli.cpp\n");
    return 2;
}
