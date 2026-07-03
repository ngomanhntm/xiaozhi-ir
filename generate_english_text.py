# -*- coding: utf-8 -*-
"""
Script to generate 1,000 clean conversational English sentences using local LM Studio.
"""

import os
import sys
import json
import requests

API_URL = "http://localhost:1234/v1/chat/completions"
OUTPUT_FILE = "D:/project/xiaozhi-esp32-main/piper_training/data/sentences_en.txt"

TOPICS = [
    "robot dog commands, movements, actions, and tricks (e.g. sit, stay, bark, fetch, dance, roll over, speed up)",
    "daily greetings, introductions, small talk, and polite conversations",
    "emotions, feelings, empathy, friendly responses, humor, jokes, and funny remarks",
    "food, cooking, favorite dishes, drinks, snacks, fruits, and dining out",
    "hobbies, leisure activities, sports, fitness, playing games, and outdoor activities",
    "music, movies, TV shows, theater, books, art, and entertainment",
    "weather, seasons, nature, environment, and earth science",
    "school, study, education, learning new languages, math, history, and science facts",
    "travel, transport, vacations, exploring new places, cities, and countries",
    "home life, family, friends, pets, household chores, and daily routines",
    "technology, computers, software, AI, internet, and the future of science",
    "time, dates, seasons, calendar events, schedules, and planning meetings",
    "general knowledge questions, interesting facts, and curious trivia about the world",
    "common expressions, idioms, proverbs, and short wisdom quotes",
    "shopping, fashion, clothes, stores, and buying items"
]

def generate_batch(count=30, topic="general chatbot phrases"):
    prompt = (
        f"Generate {count} diverse, natural, and friendly English sentences "
        f"suitable for an interactive AI chatbot on the topic of: '{topic}'. "
        "Requirements:\n"
        "1. Each sentence must be short (between 15 and 60 characters).\n"
        "2. Avoid special characters, abbreviations, or numbers if possible.\n"
        "3. Output ONLY the raw sentences, one per line. Do NOT number them, do NOT write markdown, do NOT write notes."
    )
    
    payload = {
        "model": "qwen/qwen3-vl-4b", # Will use whatever model is currently loaded in LM Studio
        "messages": [
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.7,
        "max_tokens": 2000
    }
    
    headers = {
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(API_URL, json=payload, headers=headers, timeout=180)
        response.raise_for_status()
        res_data = response.json()
        content = res_data["choices"][0]["message"]["content"].strip()
        
        # Split into lines and clean
        lines = [line.strip() for line in content.splitlines() if line.strip()]
        # Filter out lines that start with numbers (in case the model numbered them anyway)
        cleaned_lines = []
        for line in lines:
            # Remove leading numbers like "1. ", "12. "
            cleaned = json.loads(json.dumps(line)) # Ensure valid string
            cleaned = cleaned.lstrip("0123456789. -")
            if len(cleaned) >= 10 and len(cleaned) <= 70:
                cleaned_lines.append(cleaned)
                
        return cleaned_lines
    except Exception as e:
        print(f"Error calling LM Studio: {e}")
        print("Please make sure LM Studio is running and the model is loaded on port 1234.")
        return []

def main():
    print("=" * 60)
    print("LM Studio English Sentence Generator (Diverse Topics)")
    print("=" * 60)
    
    # Check if directory exists
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    
    total_needed = 1000
    all_sentences = set()
    
    # Load existing sentences if file already exists to resume
    if os.path.exists(OUTPUT_FILE):
        with open(OUTPUT_FILE, 'r', encoding='utf-8') as f:
            for line in f:
                if line.strip():
                    all_sentences.add(line.strip())
        print(f"Found {len(all_sentences)} existing sentences in {OUTPUT_FILE}.")
        
    attempts = 0
    max_attempts = 150  # Increased since we are doing smaller batches and filtering more
    
    while len(all_sentences) < total_needed and attempts < max_attempts:
        needed = total_needed - len(all_sentences)
        batch_size = min(30, needed + 5) # Ask for a bit more to account for filtering
        
        # Pick a topic dynamically based on the current attempt
        topic = TOPICS[attempts % len(TOPICS)]
        
        print(f"\nRequesting batch of {batch_size} sentences on topic '{topic}'...")
        print(f"Current total: {len(all_sentences)}/{total_needed}")
        batch = generate_batch(batch_size, topic)
        
        if not batch:
            print("Failed to get response from LM Studio. Exiting.")
            sys.exit(1)
            
        new_count = 0
        for s in batch:
            if s not in all_sentences:
                all_sentences.add(s)
                new_count += 1
                
        print(f"Added {new_count} new unique sentences in this batch.")
        attempts += 1
        
        # Write progress immediately to file
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            for s in sorted(list(all_sentences), key=len):
                f.write(s + "\n")
                
    print(f"\nCompleted! Generated {len(all_sentences)} sentences saved to {OUTPUT_FILE}.")

if __name__ == "__main__":
    import sys
    if sys.platform == "win32":
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    main()
