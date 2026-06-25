import os
import time
import requests
from pathlib import Path
from pydub import AudioSegment
from tqdm import tqdm

API_KEY = "9104ZNUwsXKZZsksWFsqaNsJcsu1MTzrYUw9fzsj" 

OUTPUT_DIR = Path("sonic_dataset")
TEMP_DIR = Path("sonic_temp")

TARGET_SR =  48000 
TARGET_CHANAL = 1
TARGET_DURATION = 8.0
TARGET_SAMPLE = int(TARGET_SR * TARGET_DURATION) #calulate full audio duration
SEARCH_QUERIES_RANDOM= [""]
SEARCH_QUERIES = [
    "rain",
    "waterfall",
    "ocean waves",
    "wind",
    "thunder",
    "fire crackling",
    "forest ambience",
    "city traffic",
    "factory machinery",
    "metal impact",
    "glass break",
    "footsteps gravel",
    "crowd noise",
    "river stream",
    "birds chirping",
    "keyboard typing",
    "engine hum",
    "church bell",
    "wood creak",
    "bubbles underwater",
] 

MAX_PER_QUERY = 150 #150 sounds per query

def search_sound(query, page = 1):
    response = requests.get(
        "https://freesound.org/apiv2/search/text/",
        params={
            "query": query,  #search word like "rain"
            "fields": "id,name,previews,duration", #we only ask Freesound for the data we actually need, not everything
            "page": page,
            "page_size": MAX_PER_QUERY,
            "filter": "duration:[1.0 TO 120.0]", #skip files that shorter then 1s and longer than 2m
            "token": API_KEY,
        },
        timeout=30
    )

    response.raise_for_status() #if something went wring it tells me what went wrong
    return response.json() #converts Freesound's response from raw text into a Python dictionary we can actually work with
    
def search_sound_rand(query, page = 1):
    response = requests.get(
        "https://freesound.org/apiv2/search/text/",
        params={
            "query": query,  #search word like "rain"
            "fields": "id,name,previews,duration", #we only ask Freesound for the data we actually need, not everything
            "page": page,
            "page_size": MAX_PER_QUERY,
            "filter": "duration:[1.0 TO 120.0]", #skip files that shorter then 1s and longer than 2m
            "sort":      "random",
            "token": API_KEY,
        },
        timeout=30
    )

    response.raise_for_status() #if something went wring it tells me what went wrong
    return response.json()

#a function that takes two things: the URL of the file to download, and dest which is where to save it on your computer
def dowload_file(url, dest): #return true or false
    try:
        response = requests.get(
            url,
            params = {"token":API_KEY},
            stream= True,
            timeout = 60,
        )
        response.raise_for_status()
        dest.parent.mkdir(parents = True, exist_ok=True) #this creates the folder if it doesn't exist yet
        with open(dest, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
              f.write(chunk)
        return True
    except Exception as e:
        print(f"Download failed: {e}")
        return False

def convert_audio(src, dst): #destination and source return true or false
    try:
        audio = AudioSegment.from_file(src)

        audio = audio.set_channels(TARGET_CHANAL)
        audio = audio.set_frame_rate(TARGET_SR)
        audio = audio.set_sample_width(2)

        target_ms = int(TARGET_DURATION*1000)

        if len(audio) > target_ms:
            audio = audio[:target_ms]
        
        
        audio.export(dst, format = "wav")
        return True
    except Exception as e:
        print(f"Convert is failed: {e}")
        return False

def main():
   # if API_KEY == "9104ZNUwsXKZZsksWFsqaNsJcsu1MTzrYUw9fzsj":
   #     print("Please set your API key first!")
   #     return
    
    OUTPUT_DIR.mkdir(parents = True, exist_ok = True)
    TEMP_DIR.mkdir(parents= True, exist_ok= True) #create folders

    seen_ids = set() #creating set(like list)
    total_saved = 0 

    answer_random = input("Write 1 if u wonna ranom and 0 if not : ")

    if answer_random == "1":
        queries = SEARCH_QUERIES_RANDOM
    elif answer_random == "0":
        queries = SEARCH_QUERIES
    else:
        print("that's not a valid option, please write 1 or 0")
        return


    for query in queries:
        print (f"Searching: {query}")
        page = 1
        query_count = 0

        while True:
            
            if answer_random == "1":
                data = search_sound_rand(query, page=page)
            else:
                data = search_sound(query, page=page)
            
            results = data.get("results",[]) #gets the list of results from Freesound's response. (id, name)

            if not results:
                break #if the results list is empty, stop paginating and move to the next query.

            
            for sound in tqdm(results, desc=query): # make interface if we will use it without tqdm it will looks like result at all
                if query_count >= MAX_PER_QUERY:  # достигли лимита?
                    break
                    
                sound_id = sound["id"]

                if sound_id in seen_ids: #if we already downloaded this ID, skip it. continue means jump to the next iteration of the loop immediately
                    continue
                seen_ids.add(sound_id)

                url = sound ["previews"]["preview-hq-mp3"] #geting url
                name = sound["name"].replace("/", "_")[:80] #geting name and cut it within 1-80 characters

                raw_path = TEMP_DIR / f"{sound_id}_{name}.mp3" #builds the path for the temporary mp3 file
                out_path = OUTPUT_DIR / f"{sound_id}_{name}.wav" #builds the path for the final WAV file

                if out_path.exists(): #if path has already exist we skip it
                    total_saved += 1
                    continue

                if not dowload_file(url, raw_path): #try to download. If it returns False (failed), skip this file
                    continue

                ok =    convert_audio(raw_path, out_path)
                if ok:
                    total_saved += 1
                    query_count += 1
                    raw_path.unlink(missing_ok=True) #deleting old path
                else:
                    raw_path.unlink(missing_ok=True) 
                
                time.sleep(0.25)

            if not data.get("next"):  #Freesound tells us if there's a next page. If not, stop paginating
                break
            page += 1
    print(f"Done! {total_saved} files saved to {OUTPUT_DIR}")

main()    