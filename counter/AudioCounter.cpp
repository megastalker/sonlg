#include <iostream>
#include <string>
#include <filesystem>
#include <queue>
#include <vector>
#include <fstream>
#include <atomic>
#include <functional>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <mutex>
#include <thread>
#include <cmath>
#include "kiss_fft.h"

namespace fs = std::filesystem;

enum FeatureIndex
{
    RMS,                // средняя энергия/громкость сигнала

    PEAK_AMPLITUDE,     // максимальная амплитуда сигнала

    ZERO_CROSSING_RATE, // насколько часто сигнал пересекает 0
                        // помогает отличать шум от тона

    SPECTRAL_CENTROID,  // "центр тяжести" спектра
                        // чем выше значение, тем ярче звук

    SPECTRAL_ROLLOFF,   // частота, ниже которой находится
                        // примерно 85% энергии сигнала

    SPECTRAL_BANDWIDTH, // разброс спектра относительно центроида
                        // показывает насколько спектр широкий

    SPECTRAL_FLATNESS,  // степень шумности сигнала
                        // 0 = чистый тон
                        // 1 = белый шум

    LOW_ENERGY_RATIO,   // доля энергии в низких частотах
                        // примерно 20-250 Hz

    MID_ENERGY_RATIO,   // доля энергии в средних частотах
                        // примерно 250-4000 Hz

    HIGH_ENERGY_RATIO,  // доля энергии в высоких частотах
                        // примерно >4000 Hz

    FEATURE_COUNT
};
struct AudioFeatures{
std::string name;
std::vector<double> value = std::vector<double>(FEATURE_COUNT, 0.0);
};
 class counter{
    public:
    fs::path Path2AudioFolder;
    drwav wav;
    int channels;
    int SampleRate;
    const int N = 4096;
    std::vector<float> samples;
    std::vector<std::vector<double>> Mag;
    // std::vector<double> Mag2;
    const double PI = 3.14159265358979323846;
    counter(fs::path p):Path2AudioFolder(p){ if( !drwav_init_file(&wav,Path2AudioFolder.string().c_str(),NULL))
        {std::cout<<"wav open fail";return;}
        SampleRate=wav.sampleRate;
        channels=wav.channels;
        samples.resize(wav.totalPCMFrameCount * wav.channels);
        drwav_read_pcm_frames_f32(&wav,wav.totalPCMFrameCount,samples.data());
        FFT();
        // int kol=0;
        // for(int i=0;i<Mag.size();i++){
        //     for(int j=0;j<Mag[i].size();j++){
        //         Mag2[kol]=Mag[i][j];
        //         kol++;
        //     }
        // }

    }
    ~counter(){ drwav_uninit(&wav);}
    void FFT()
    {
        //const int N = 4096;

        Mag.clear();

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

            for(int i = 0; i < N / 2; i++)
            {
                windowMag[i] = std::sqrt(
                    out[i].r * out[i].r +
                    out[i].i * out[i].i
                );
            }

            Mag.push_back(windowMag);
        }

        free(cfg);
    }

        std::vector<double> count(){
            std::vector<double> v;
            if (samples.size()<2){std::cout<<"wav file problem";return v;}
            v.resize(FEATURE_COUNT);
            v[RMS]=RMS_();
            v[PEAK_AMPLITUDE]=PEAK_AMPLITUDE_();
            v[ZERO_CROSSING_RATE]=ZERO_CROSSING_RATE_();
            v[SPECTRAL_CENTROID]=SPECTRAL_CENTROID_();
            v[SPECTRAL_ROLLOFF]=SPECTRAL_ROLLOFF_();
            v[SPECTRAL_BANDWIDTH]=SPECTRAL_BANDWIDTH_();
            v[SPECTRAL_FLATNESS]=SPECTRAL_FLATNESS_();
            v[LOW_ENERGY_RATIO]=LOW_ENERGY_RATIO_();
            v[MID_ENERGY_RATIO]=MID_ENERGY_RATIO_();
            v[HIGH_ENERGY_RATIO]=HIGH_ENERGY_RATIO_();

            return v;

        }
        double RMS_(){double sum =0; for(float sample:samples){sum+=sample*sample;}return sqrt((sum/samples.size()));}
        double PEAK_AMPLITUDE_(){double max=-1;for(float sample:samples){if(max<abs(sample))max=abs(sample);}return max;}

        double ZERO_CROSSING_RATE_(){
            int rate=0;
            for(size_t i = 1;i<samples.size();i++){
                    if(samples[i]*samples[i-1]<0)rate++;
            }
            return (double)rate/(samples.size()-1);
        }
        double SPECTRAL_CENTROID_(){
           double windowsum=0;
            for(size_t i =0;i<Mag.size();i++){
                double up=0;
                double low=0;
                for(size_t j = 0;j<Mag[i].size();j++){
                    up+=Mag[i][j]*(double)j*(double)SampleRate/N;
                    low+=Mag[i][j];
                }
                if(low!=0)
                windowsum+=(up/low);
            }
            return windowsum/Mag.size();
            
        }
        double SPECTRAL_ROLLOFF_(){
            double WindowTotal=0;
            for(int i =0;i<Mag.size();i++){
                double total=0;
                for(int j=0;j<Mag[i].size();j++){
                    total+=Mag[i][j];
                }
                int r = 0;
                double sum=0;
                for(r=0;sum<0.85*total;r++){
                    sum+=Mag[i][r];
                }
                WindowTotal+=(double)(r*SampleRate)/N;
            }
            return WindowTotal/Mag.size();
        }
        double SPECTRAL_BANDWIDTH_(){
            double windowsum=0;
            for(size_t i =0;i<Mag.size();i++){
                double up=0;
                double low=0;
                for(size_t j = 0;j<Mag[i].size();j++){
                    up+=Mag[i][j]*(double)j*(double)SampleRate/N;
                    low+=Mag[i][j];
                }
                double c = 0;
                if(low!=0)
                    c =(up/low);
                up=0;
                low=0;
                for(int j=0;j<Mag[i].size();j++){
                    up+=Mag[i][j]*((double)j*SampleRate/N-c)*((double)j*SampleRate/N-c);
                    low+=Mag[i][j];
                }
                if(low!=0)
                windowsum+=sqrt(up/low);
            }
            return windowsum/Mag.size();
            
        }
        double SPECTRAL_FLATNESS_(){
            double winsum=0;
            double E=1e-12;
            for(int i = 0;i<Mag.size();i++){
                double up=0;
                double low=0;
                int j=0;
                for(j=0;j<Mag[i].size();j++){
                    up+=log(Mag[i][j]+E);
                    low+=Mag[i][j];
                }
                if(low!=0)
                winsum+=exp(up/j)/(low/j);
            }
        return winsum/Mag.size();
        }
        double LOW_ENERGY_RATIO_(){
            double WindowSum=0;
            for(int i=0;i<Mag.size();i++){
                double up=0;
                double low=0;
                for(int j=0;j<Mag[i].size();j++){
                    double Freq=(double)j*SampleRate/N;
                    if(Freq>=20 && Freq<=250)up+=Mag[i][j];
                    low+=Mag[i][j];
                }
                if(low!=0)
                WindowSum+=up/low;

            }
            return WindowSum/Mag.size();
        }
        double MID_ENERGY_RATIO_(){
            double WindowSum=0;
            for(int i=0;i<Mag.size();i++){
                double up=0;
                double low=0;
                for(int j=0;j<Mag[i].size();j++){
                    double Freq=(double)j*SampleRate/N;
                    if(Freq>=250 && Freq<=4000)up+=Mag[i][j];
                    low+=Mag[i][j];
                }
                if(low!=0)
                WindowSum+=up/low;

            }
            return WindowSum/Mag.size();
        }
        double HIGH_ENERGY_RATIO_(){
            double WindowSum=0;
            for(int i=0;i<Mag.size();i++){
                double up=0;
                double low=0;
                for(int j=0;j<Mag[i].size();j++){
                    double Freq=(double)j*SampleRate/N;
                    if(Freq>=4000)up+=Mag[i][j];
                    low+=Mag[i][j];
                }
                if(low!=0)
                WindowSum+=up/low;

            }
            return WindowSum/Mag.size();
        }


};
    //  RMS          
    //  PEAK_AMPLITUDE   
    //  ZERO_CROSSING_RATE
    //  SPECTRAL_CENTROID
    //  SPECTRAL_ROLLOFF
    //  SPECTRAL_BANDWIDTH
    //  SPECTRAL_FLATNESS
    //  OW_ENERGY_RATIO
    //  MID_ENERGY_RATIO
    //  HIGH_ENERGY_RATIO
    //  FEATURE_COUNT



class head{ //будет принимать относительный путь да и в целом делать +- все
    fs::path Path2AudioFolder;
    std::queue<AudioFeatures> qu;
    std::mutex mutex;
    //std::atomic_bool done = false;
    int FileInFolder = 0;
    public:
    head(std::string link):Path2AudioFolder(link){
        auto it = fs::directory_iterator(Path2AudioFolder);
        auto end = fs::directory_iterator();
        while(it!=end){
            it++;
            FileInFolder++;
        }
        std::cout<<FileInFolder<<" number files in audio folder\n";
    }
    void constructor(){
        std::vector<fs::path> AllPath;
        AllPath.reserve(FileInFolder);
        for(auto &entry:fs::directory_iterator(Path2AudioFolder)){
            AllPath.push_back(entry.path());
        }
        std::atomic_int How_Many_files_read=0;
        std::vector<std::thread> t;
        for(int i=0;i<8;i++){
            t.emplace_back(&head::Thrd_Features,this,std::ref(AllPath),std::ref(How_Many_files_read));
        }
        for(int i = 0;i<8;i++){
            t[i].join();
        }
        std::vector<AudioFeatures> All;
        while(!qu.empty()){
            All.push_back(qu.front());
            qu.pop();
        }
        PrintValueInTxt(All);
    }
    AudioFeatures FeatureCount(fs::path Path2File){ 
        //drwav wav;
        AudioFeatures features;
        //if( !drwav_init_file(&wav,Path2File.string().c_str(),NULL))
        //{std::cout<<"wav open fail"; return features;}
        features.name = Path2File.filename().string();
        /// считаем хуйню
        counter c(Path2File);
        features.value=c.count();
        // std::lock_guard<std::mutex> lock(mutex);
        // qu.push(features);
        //drwav_uninit(&wav);
        return features;
    }
    void Thrd_Features(const std::vector<fs::path> &v,std::atomic_int &How_Many_Read){
        AudioFeatures a;
        while(1){
            int index = How_Many_Read++;
            if(index>=FileInFolder)break;
            a = FeatureCount(v[index]);
            std::lock_guard<std::mutex> lock(mutex);
            qu.push(a);
        }
    }
    void PrintValueInTxt(const std::vector<AudioFeatures> &AllValue){
        std::ofstream out("../AllAudioFeatures.txt");
        if (!out){
            std::cout << "Error opening txt file\n";
            return;
            }  
            
        for(size_t i = 0;i<AllValue.size();i++){
            out << AllValue[i].name<<":";
            for(size_t j = 0;j<FEATURE_COUNT;j++){
                out << AllValue[i].value[j]<<",";
            }
            out << "\n";
        }
    }

};

int main(){
head h("../data/audio");
h.constructor();
    return 0;
}