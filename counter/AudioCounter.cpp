#include <iostream>
#include <string>
#include <filesystem>
#include <queue>
#include <vector>
#include <fstream>
#include <atomic>
#include <mutex>
#include <thread>
#include <cmath>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "kiss_fft.h"

namespace fs = std::filesystem;

enum FeatureIndex
{
    RMS,
    PEAK_AMPLITUDE,
    ZERO_CROSSING_RATE,
    SPECTRAL_CENTROID,
    SPECTRAL_ROLLOFF,
    SPECTRAL_BANDWIDTH,
    SPECTRAL_FLATNESS,
    LOW_ENERGY_RATIO,
    MID_ENERGY_RATIO,
    HIGH_ENERGY_RATIO,
    FEATURE_COUNT
};

struct AudioFeatures
{
    std::string name;
    std::vector<double> value = std::vector<double>(FEATURE_COUNT, 0.0);
};

class counter
{
public:
    fs::path Path2AudioFile;
    drwav wav{};

    int channels = 0;
    int SampleRate = 0;
    const int N = 4096;

    std::vector<float> samples;
    std::vector<std::vector<double>> Mag;

    std::vector<double> WindowEnergy;
    std::vector<double> BinFreq;

    counter(fs::path p) : Path2AudioFile(p)
    {
        if(!drwav_init_file(&wav, Path2AudioFile.string().c_str(), NULL))
        {
            std::cout << "wav open fail: " << Path2AudioFile << "\n";
            return;
        }

        SampleRate = wav.sampleRate;
        channels = wav.channels;

        samples.resize(wav.totalPCMFrameCount * wav.channels);
        drwav_read_pcm_frames_f32(
            &wav,
            wav.totalPCMFrameCount,
            samples.data()
        );

        InitBinFreq();
        FFT();
    }

    ~counter()
    {
        drwav_uninit(&wav);
    }

    void InitBinFreq()
    {
        BinFreq.resize(N / 2);

        for(int i = 0; i < N / 2; i++)
        {
            BinFreq[i] = static_cast<double>(i) * SampleRate / N;
        }
    }

    void FFT()
    {
        Mag.clear();
        WindowEnergy.clear();

        if(samples.size() < N)
            return;

        kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, nullptr, nullptr);

        if(!cfg)
            return;

        for(size_t start = 0; start + N <= samples.size(); start += N)
        {
            std::vector<kiss_fft_cpx> in(N);
            std::vector<kiss_fft_cpx> out(N);

            for(int i = 0; i < N; i++)
            {
                in[i].r = samples[start + i];
                in[i].i = 0.0f;
            }

            kiss_fft(cfg, in.data(), out.data());

            std::vector<double> windowMag(N / 2);
            double totalEnergy = 0.0;

            for(int i = 0; i < N / 2; i++)
            {
                windowMag[i] = std::sqrt(
                    out[i].r * out[i].r +
                    out[i].i * out[i].i
                );

                totalEnergy += windowMag[i];
            }

            Mag.push_back(windowMag);
            WindowEnergy.push_back(totalEnergy);
        }

        free(cfg);
    }

    std::vector<double> count()
    {
        std::vector<double> v(FEATURE_COUNT, 0.0);

        if(samples.size() < 2)
        {
            std::cout << "wav file problem\n";
            return v;
        }

        v[RMS] = RMS_();
        v[PEAK_AMPLITUDE] = PEAK_AMPLITUDE_();
        v[ZERO_CROSSING_RATE] = ZERO_CROSSING_RATE_();

        if(!Mag.empty())
        {
            v[SPECTRAL_CENTROID] = SPECTRAL_CENTROID_();
            v[SPECTRAL_ROLLOFF] = SPECTRAL_ROLLOFF_();
            v[SPECTRAL_BANDWIDTH] = SPECTRAL_BANDWIDTH_();
            v[SPECTRAL_FLATNESS] = SPECTRAL_FLATNESS_();
            v[LOW_ENERGY_RATIO] = LOW_ENERGY_RATIO_();
            v[MID_ENERGY_RATIO] = MID_ENERGY_RATIO_();
            v[HIGH_ENERGY_RATIO] = HIGH_ENERGY_RATIO_();
        }

        return v;
    }

    double RMS_()
    {
        double sum = 0.0;

        for(float sample : samples)
            sum += sample * sample;

        return std::sqrt(sum / samples.size());
    }

    double PEAK_AMPLITUDE_()
    {
        double peak = 0.0;

        for(float sample : samples)
        {
            double a = std::abs(sample);
            if(peak < a)
                peak = a;
        }

        return peak;
    }

    double ZERO_CROSSING_RATE_()
    {
        int rate = 0;

        for(size_t i = 1; i < samples.size(); i++)
        {
            if(samples[i] * samples[i - 1] < 0)
                rate++;
        }

        return static_cast<double>(rate) / (samples.size() - 1);
    }

    double SPECTRAL_CENTROID_()
    {
        double windowSum = 0.0;
        int validWindows = 0;

        for(size_t i = 0; i < Mag.size(); i++)
        {
            if(WindowEnergy[i] == 0.0)
                continue;

            double up = 0.0;

            for(size_t j = 0; j < Mag[i].size(); j++)
                up += Mag[i][j] * BinFreq[j];

            windowSum += up / WindowEnergy[i];
            validWindows++;
        }

        if(validWindows == 0)
            return 0.0;

        return windowSum / validWindows;
    }

    double SPECTRAL_ROLLOFF_()
    {
        double windowTotal = 0.0;
        int validWindows = 0;

        for(size_t i = 0; i < Mag.size(); i++)
        {
            if(WindowEnergy[i] == 0.0)
                continue;

            double target = 0.85 * WindowEnergy[i];
            double sum = 0.0;
            size_t r = 0;

            for(; r < Mag[i].size(); r++)
            {
                sum += Mag[i][r];

                if(sum >= target)
                    break;
            }

            if(r >= BinFreq.size())
                r = BinFreq.size() - 1;

            windowTotal += BinFreq[r];
            validWindows++;
        }

        if(validWindows == 0)
            return 0.0;

        return windowTotal / validWindows;
    }

    double SPECTRAL_BANDWIDTH_()
    {
        double windowSum = 0.0;
        int validWindows = 0;

        for(size_t i = 0; i < Mag.size(); i++)
        {
            if(WindowEnergy[i] == 0.0)
                continue;

            double centroidUp = 0.0;

            for(size_t j = 0; j < Mag[i].size(); j++)
                centroidUp += Mag[i][j] * BinFreq[j];

            double centroid = centroidUp / WindowEnergy[i];

            double up = 0.0;

            for(size_t j = 0; j < Mag[i].size(); j++)
            {
                double diff = BinFreq[j] - centroid;
                up += Mag[i][j] * diff * diff;
            }

            windowSum += std::sqrt(up / WindowEnergy[i]);
            validWindows++;
        }

        if(validWindows == 0)
            return 0.0;

        return windowSum / validWindows;
    }

    double SPECTRAL_FLATNESS_()
    {
        double windowSum = 0.0;
        int validWindows = 0;

        const double E = 1e-12;

        for(size_t i = 0; i < Mag.size(); i++)
        {
            if(WindowEnergy[i] == 0.0)
                continue;

            double logSum = 0.0;
            size_t K = Mag[i].size();

            for(size_t j = 0; j < K; j++)
                logSum += std::log(Mag[i][j] + E);

            double geometricMean = std::exp(logSum / K);
            double arithmeticMean = WindowEnergy[i] / K;

            if(arithmeticMean != 0.0)
            {
                windowSum += geometricMean / arithmeticMean;
                validWindows++;
            }
        }

        if(validWindows == 0)
            return 0.0;

        return windowSum / validWindows;
    }

    double ENERGY_RATIO_RANGE(double minFreq, double maxFreq)
    {
        double windowSum = 0.0;
        int validWindows = 0;

        for(size_t i = 0; i < Mag.size(); i++)
        {
            if(WindowEnergy[i] == 0.0)
                continue;

            double bandEnergy = 0.0;

            for(size_t j = 0; j < Mag[i].size(); j++)
            {
                if(BinFreq[j] >= minFreq && BinFreq[j] < maxFreq)
                    bandEnergy += Mag[i][j];
            }

            windowSum += bandEnergy / WindowEnergy[i];
            validWindows++;
        }

        if(validWindows == 0)
            return 0.0;

        return windowSum / validWindows;
    }

    double LOW_ENERGY_RATIO_()
    {
        return ENERGY_RATIO_RANGE(20.0, 250.0);
    }

    double MID_ENERGY_RATIO_()
    {
        return ENERGY_RATIO_RANGE(250.0, 4000.0);
    }

    double HIGH_ENERGY_RATIO_()
    {
        return ENERGY_RATIO_RANGE(4000.0, SampleRate / 2.0);
    }
};

class head
{
    fs::path Path2AudioFolder;
    std::queue<AudioFeatures> qu;
    std::mutex mutex;
    int FileInFolder = 0;

public:
    head(std::string link) : Path2AudioFolder(link)
    {
        for(auto& entry : fs::directory_iterator(Path2AudioFolder))
        {
            if(entry.is_regular_file())
                FileInFolder++;
        }

        std::cout << FileInFolder << " number files in audio folder\n";
    }

    void constructor()
    {
        std::vector<fs::path> AllPath;
        AllPath.reserve(FileInFolder);

        for(auto& entry : fs::directory_iterator(Path2AudioFolder))
        {
            if(entry.is_regular_file())
                AllPath.push_back(entry.path());
        }

        std::atomic_int How_Many_files_read = 0;
        std::vector<std::thread> t;

        int threadCount = 8;

        for(int i = 0; i < threadCount; i++)
        {
            t.emplace_back(
                &head::Thrd_Features,
                this,
                std::cref(AllPath),
                std::ref(How_Many_files_read)
            );
        }

        for(auto& th : t)
            th.join();

        std::vector<AudioFeatures> All;

        while(!qu.empty())
        {
            All.push_back(qu.front());
            qu.pop();
        }

        PrintValueInTxt(All);
    }

    AudioFeatures FeatureCount(fs::path Path2File)
    {
        AudioFeatures features;
        features.name = Path2File.filename().string();

        counter c(Path2File);
        features.value = c.count();

        return features;
    }

    void Thrd_Features(
        const std::vector<fs::path>& v,
        std::atomic_int& How_Many_Read
    )
    {
        while(true)
        {
            int index = How_Many_Read++;

            if(index >= static_cast<int>(v.size()))
                break;

            AudioFeatures a = FeatureCount(v[index]);

            std::lock_guard<std::mutex> lock(mutex);
            qu.push(a);
        }
    }

    void PrintValueInTxt(const std::vector<AudioFeatures>& AllValue)
    {
        std::ofstream out("../AllAudioFeatures.txt");

        if(!out)
        {
            std::cout << "Error opening txt file\n";
            return;
        }

        for(size_t i = 0; i < AllValue.size(); i++)
        {
            out << AllValue[i].name << ":";

            for(size_t j = 0; j < FEATURE_COUNT; j++)
                out << AllValue[i].value[j] << ",";

            out << "\n";
        }
    }
};

int main()
{
    head h("../data/audio");
    h.constructor();

    return 0;
}