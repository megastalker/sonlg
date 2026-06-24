#include <iostream>
#include <string>
#include <filesystem>
#include <queue>
#include <vector>
#include <fstream>
#include <atomic>
#include <functional>
#include "dr_wav.h"
#include <mutex>
#include <thread>
#include <math.h>
#include <complex>
#include <cmath>
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
        drwav wav;
        AudioFeatures features;
        //if( !drwav_init_file(&wav,Path2File.string().c_str(),NULL))
        //{std::cout<<"wav open fail"; return features;}
        features.name = Path2File.filename().string();
        /// считаем хуйню
        //std::lock_guard<std::mutex> lock(mutex);
        //qu.push(features);
        //drwav_uninit(&wav);
        //return features;
    }
    void Thrd_Features(const std::vector<fs::path> &v,std::atomic_int &How_Many_Read){
        AudioFeatures a;
        while(1){
            int index = How_Many_Read++;
            if(index<FileInFolder)break;
            a = FeatureCount(v[index]);
            How_Many_Read++;
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
 class counter{
    fs::path Path2AudioFolder;
    std::queue<AudioFeatures> qu;
    drwav wav;
    int channels;
    int SampleRate;
    std::vector<float> samples;
    std::vector<double> RE;
    std::vector<double> IM;
    std::vector<double> Mag;
    const double PI = 3.14159265358979323846;
    counter(fs::path p,std::queue<AudioFeatures> q):Path2AudioFolder(p),qu(q){ if( !drwav_init_file(&wav,Path2AudioFolder.string().c_str(),NULL))
        {std::cout<<"wav open fail";return;}
        SampleRate=wav.sampleRate;
        channels=wav.channels;
        samples.resize(wav.totalPCMFrameCount * wav.channels);
        drwav_read_pcm_frames_f32(&wav,wav.totalPCMFrameCount,samples.data());

    }
    ~counter(){ drwav_uninit(&wav);}
        void FFT(){
            // for(int k = 0; k<samples.size();k++){
            //     double ReSum=0;
            //     double ImSum=0;
            //     for(int n = 0;n<samples.size();n++){
            //         ReSum+=samples[n]*cos((2* PI *k*n)/n);
            //         ImSum-=samples[n]*sin((2* PI *k*n)/n);
            //     }
            //     RE[k]=ReSum;
            //     IM[k]=ImSum;
            //     Mag[k]=sqrt((ReSum*ReSum)+(ImSum+ImSum));
            // }
        }

        void count(){
            if (samples.size()<2){std::cout<<"wav file problem";return;}

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
        double SPECTRAL_CENTROID_(){}
        double SPECTRAL_ROLLOFF_(){}
        double SPECTRAL_BANDWIDTH_(){}
        double SPECTRAL_FLATNESS(){}
        double OW_ENERGY_RATIO(){}
        double MID_ENERGY_RATIO(){}
        double HIGH_ENERGY_RATIO(){}
        double FEATURE_COUNT(){}


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



int main(){

head h("../data/audio");


    return 0;
}