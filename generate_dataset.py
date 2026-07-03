import os
import sys
import time
import requests
import torchaudio

# ==============================================================================
# CONFIGURATION
# ==============================================================================
API_KEYS = [
    "sk_live_YOUR_KEY_1",
    "sk_live_YOUR_KEY_2",
]
VOICE_ID = "8VXsCLxU7Pn55ADXQc6sAb"       # Paste your VClip Voice ID here
TEXT_FILE = "D:/project/xiaozhi-esp32-main/piper_training/data/sentences.txt"      # Path to existing sentences
OUTPUT_DIR = "D:/project/xiaozhi-esp32-main/piper_training/dataset_adam"           # Target directory for Adam voice
SAMPLE_RATE = 22050              # Standard Piper sample rate
# ==============================================================================

def convert_mp3_to_wav(mp3_path, wav_path, target_sr=22050):
    try:
        # Load the downloaded MP3 using torchaudio
        waveform, sr = torchaudio.load(mp3_path)
        
        # Convert stereo to mono if necessary
        if waveform.shape[0] > 1:
            waveform = waveform.mean(dim=0, keepdim=True)
            
        # Resample to the target sample rate (22050Hz for Piper)
        if sr != target_sr:
            resampler = torchaudio.transforms.Resample(orig_freq=sr, new_freq=target_sr)
            waveform = resampler(waveform)
            
        # Save as 16-bit Signed PCM WAV
        torchaudio.save(wav_path, waveform, target_sr, bits_per_sample=16, encoding="PCM_S")
        return True
    except Exception as e:
        print(f"Error converting {mp3_path}: {e}")
        return False

def generate_voice(text, api_key, voice_id):
    url = "https://api-tts.vclip.io/json-rpc"
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    payload = {
        "method": "ttsLongText",
        "input": {
            "text": text,
            "userVoiceId": voice_id,
            "speed": 1.0
        }
    }
    
    # 1. Trigger speech generation
    response = requests.post(url, headers=headers, json=payload)
    response.raise_for_status()
    res_data = response.json()
    
    if "error" in res_data:
        raise RuntimeError(f"VClip API Error: {res_data['error']}")
        
    # Handle JSON-RPC result format variations
    result = res_data.get("result", {})
    if isinstance(result, str):
        export_id = result
    else:
        export_id = result.get("projectExportId")
        
    if not export_id:
        raise ValueError(f"Could not find projectExportId in API response: {res_data}")
        
    # 2. Poll export status
    status_payload = {
        "method": "getExportStatus",
        "input": {
            "projectExportId": export_id
        }
    }
    
    attempts = 0
    max_attempts = 30 # Up to 1 minute of polling per sentence
    while attempts < max_attempts:
        time.sleep(2)
        status_resp = requests.post(url, headers=headers, json=status_payload)
        status_resp.raise_for_status()
        status_data = status_resp.json()
        
        if "error" in status_data:
            raise RuntimeError(f"VClip Status API Error: {status_data['error']}")
            
        status_result = status_data.get("result", {})
        state = status_result.get("state")
        
        if state == "completed":
            audio_url = status_result.get("url")
            if not audio_url:
                raise ValueError("Completed status received but no audio URL found.")
            return audio_url
        elif state == "failed":
            raise RuntimeError(f"VClip audio generation failed: {status_result}")
            
        attempts += 1
        
    raise TimeoutError("Audio generation timed out on VClip server.")

def main():
    # Filter out empty or placeholder keys
    active_keys = [k for k in API_KEYS if k and not k.startswith("sk_live_...") and k != "YOUR_API_KEY"]
    if not active_keys:
        print("Error: Please add at least one valid VClip API key to the API_KEYS list.")
        sys.exit(1)
        
    if VOICE_ID == "YOUR_VOICE_ID" or not VOICE_ID:
        print("Error: Please replace VOICE_ID with your actual VClip Voice ID.")
        sys.exit(1)

    if not os.path.exists(TEXT_FILE):
        print(f"Error: Text file '{TEXT_FILE}' not found. Please create it and add one sentence per line.")
        sys.exit(1)
        
    # Create directories
    wavs_dir = os.path.join(OUTPUT_DIR, "wavs")
    temp_dir = os.path.join(OUTPUT_DIR, "temp")
    os.makedirs(wavs_dir, exist_ok=True)
    os.makedirs(temp_dir, exist_ok=True)
    
    metadata_path = os.path.join(OUTPUT_DIR, "metadata.csv")
    
    # Load already processed sentences to support resuming
    processed_texts = set()
    next_index = 1
    
    if os.path.exists(metadata_path):
        with open(metadata_path, 'r', encoding='utf-8') as f:
            for line in f:
                parts = line.strip().split('|')
                if len(parts) >= 2:
                    processed_texts.add(parts[1])
                    # Parse index from filename (e.g. wavs/0005.wav -> 5)
                    try:
                        filename = parts[0]
                        basename = os.path.basename(filename)
                        idx = int(basename.split('.')[0])
                        if idx >= next_index:
                            next_index = idx + 1
                    except ValueError:
                        pass
        print(f"Resuming dataset generation. Found {len(processed_texts)} already processed sentences.")
        
    # Read sentences to process
    with open(TEXT_FILE, 'r', encoding='utf-8') as f:
        raw_sentences = [line.strip() for line in f if line.strip()]
        
    # Filter out sentences that are too short/long or contain low quality keywords
    # Keep already processed ones to maintain consistency in index/resuming
    exclude_keywords = [
        'loài này', 'mô tả khoa học', 'tiểu hành tinh', 'phát hiện', 'phân bố', 
        'đặc hữu', 'xã thuộc', 'huyện thuộc', 'tỉnh thuộc', 'dân số', 'diện tích', 
        'giáp các đô thị', 'bản địa của', 'đô thị này', 'sải cánh', 'thuộc chi', 
        'miêu tả khoa học', 'thuộc họ', 'loài thảo mộc', 'loài thực vật'
    ]
    sentences = []
    for s in raw_sentences:
        if s in processed_texts:
            sentences.append(s)
        else:
            if len(s) >= 25 and len(s) <= 65 and not any(k in s.lower() for k in exclude_keywords):
                sentences.append(s)
                
    # Sort sentences by length (shortest first) to maximize sentence count per credit
    sentences.sort(key=len)
    total_sentences = len(sentences)
    print(f"Total clean sentences matching criteria: {total_sentences} (sorted shortest-first)")
    
    # Filter out sentences that have already been generated
    sentences_to_process = [s for s in sentences if s not in processed_texts]
    print(f"Sentences left to process: {len(sentences_to_process)}")
    
    success_count = 0
    fail_count = 0
    current_key_idx = 0
    
    # Open metadata file in append mode
    with open(metadata_path, 'a', encoding='utf-8', buffering=1) as meta_file:
        for i, sentence in enumerate(sentences_to_process):
            if next_index > 1000:
                print("\n[INFO] Reached 1000 audio files limit! Stopping dataset generation.")
                break
            file_id = f"{next_index:04d}"
            temp_mp3 = os.path.join(temp_dir, f"{file_id}.mp3")
            target_wav = os.path.join(wavs_dir, f"{file_id}.wav")
            relative_wav = f"wavs/{file_id}.wav"
            
            print(f"\n[{i+1}/{len(sentences_to_process)}] Processing: '{sentence}' (Len: {len(sentence)} chars)")
            
            success = False
            # Attempt to process using rotated keys
            while current_key_idx < len(active_keys):
                api_key = active_keys[current_key_idx]
                try:
                    # 1. Generate via VClip API
                    audio_url = generate_voice(sentence, api_key, VOICE_ID)
                    
                    # 2. Download temporary MP3
                    print(f"Downloading audio stream using Key #{current_key_idx+1}...")
                    audio_resp = requests.get(audio_url)
                    audio_resp.raise_for_status()
                    with open(temp_mp3, 'wb') as f:
                        f.write(audio_resp.content)
                        
                    # 3. Convert to Piper WAV format
                    print("Converting MP3 to WAV (22050Hz Mono)...")
                    convert_success = convert_mp3_to_wav(temp_mp3, target_wav, SAMPLE_RATE)
                    
                    # Cleanup temp file
                    if os.path.exists(temp_mp3):
                        os.remove(temp_mp3)
                        
                    if convert_success:
                        # 4. Write to metadata
                        meta_file.write(f"{relative_wav}|{sentence}\n")
                        processed_texts.add(sentence)
                        next_index += 1
                        success_count += 1
                        print(f"Success! Saved to {target_wav}")
                        success = True
                        break  # Break retry loop, proceed to next sentence
                    else:
                        print("Conversion failed. Retrying with next key...")
                        current_key_idx += 1
                        
                except Exception as e:
                    error_msg = str(e)
                    print(f"Key #{current_key_idx+1} failed: {error_msg}")
                    
                    # Clean up temp file if exists
                    if os.path.exists(temp_mp3):
                        os.remove(temp_mp3)
                        
                    # Rotate key on credit limit/auth/billing errors
                    if any(word in error_msg.lower() for word in ["credit", "billing", "limit", "auth", "unauthorized", "token"]):
                        print(f"--> Key #{current_key_idx+1} ran out of credits or has auth issues. Rotating to Key #{current_key_idx+2}...")
                        current_key_idx += 1
                    else:
                        # For network timeout or other transient errors, retry with next key to be safe
                        print(f"--> Transient error. Rotating key to try again...")
                        current_key_idx += 1
            
            if not success:
                print(f"Failed to process sentence after trying all keys: {sentence}")
                fail_count += 1
                if current_key_idx >= len(active_keys):
                    print("\n[CRITICAL] All API keys have been exhausted! Stopping dataset generation.")
                    break
                    
            # Polite sleep between generations to respect API limits
            time.sleep(1)
            
    print("\n==============================================================================")
    print(f"Dataset generation complete!")
    print(f"Success: {success_count} files.")
    print(f"Failed: {fail_count} files.")
    print(f"WAVs saved to: {wavs_dir}")
    print(f"Metadata file saved to: {metadata_path}")
    print("==============================================================================")

if __name__ == "__main__":
    main()
