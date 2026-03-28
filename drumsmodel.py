import os 
import json 
import time 
import torch # used to work with tensors, it is used for sentence embeddings
from sentence_transformers import SentenceTransformer, util # used to work with sentence embeddings, It is used for semantic similarity
import PyPDF2 # used to work with PDF files, it is used to extract text from PDF files
from openpyxl import Workbook # used to work with Excel files, It is used to create Excel files
import shutil # used to work with files, It is used to copy files
import nltk # used to work with text, It is used for natural language processing and used in preprocessing, specifically for tokenization, stopword removal, and stemming
import string 
from sklearn.feature_extraction.text import TfidfVectorizer # TfidfVectorizer is a class that is used to work with text, It is used for TF-IDF vectorization
from nltk.corpus import stopwords # stopwords is a module that is used to work with text, It is used for removing stopwords
import nltk.stem.porter # porter is a module that is used to work with text, It is used for stemming
from sklearn.metrics.pairwise import cosine_similarity # cosine_similarity is a class that is used to work with text, It is used for calculating cosine similarity
import math
import numpy as np
import io 
from PIL import Image
import easyocr # used to extract text from images

# Global OCR instance for lazy loading
_ocr_instance = None

def get_ocr_instance():
    global _ocr_instance
    if _ocr_instance is None:
        print("Initializing EasyOCR...")
        # 'en' for English. EasyOCR is often faster and lighter.
        _ocr_instance = easyocr.Reader(['en'], gpu=True) 
    return _ocr_instance

# Download necessary NLTK data
nltk.download('stopwords')
ps = nltk.stem.porter.PorterStemmer()
vectorizer = TfidfVectorizer()

# Configuration
TASK_DIR = "."  # Directory where JSON tasks are saved
COMPLETED_DIR = "completed_tasks"
OUTPUT_DIR = "folderUSER/rtimgsUSER" # As per user request for xlsx
MODEL_PATH = './offline_model'

# Ensure directories exist
os.makedirs(COMPLETED_DIR, exist_ok=True)
os.makedirs(OUTPUT_DIR, exist_ok=True)

def extract_text_from_file(file_path):
    """Extracts text from a PDF or Image file."""
    if not os.path.exists(file_path):
        return ""
    
    ext = file_path.lower().split('.')[-1]
    
    if ext == 'pdf':
        text = ""
        try:
            with open(file_path, "rb") as file:
                reader = PyPDF2.PdfReader(file)
                for page in reader.pages:
                    extracted = page.extract_text()
                    if extracted:
                        text += extracted + "\n"
        except Exception as e:
            print(f"Error reading PDF {file_path}: {e}")
        return text
    
    elif ext in ['png', 'jpg', 'jpeg', 'bmp', 'tiff']:
        print(f"Performing OCR on image: {file_path}")
        try:
            reader = get_ocr_instance()
            # EasyOCR can handle file paths directly
            results = reader.readtext(file_path)
            
            lines = []
            if results:
                for (bbox, text, confidence) in results:
                    lines.append(text)
            return "\n".join(lines)
        except Exception as e:
            print(f"Error performing OCR on {file_path}: {e}")
            return ""
    
    else:
        print(f"Unsupported file format: {ext}")
        return ""

# Preprocessing from eval.py
def toLower(text):
    if not isinstance(text, str):
        text = str(text)
    return text.lower()

def remove_punc(text):
    exclude = string.punctuation
    if not isinstance(text, str):
        text = str(text)
    return text.translate(str.maketrans('','',exclude))

def remove_stopword(text):
    new_text = []
    # Ensure stopwords are available
    stop_words = set(stopwords.words('english'))
    for word in text.split():
        if word in stop_words:
            new_text.append('')
        else:
            new_text.append(word)
    return " ".join(new_text)
              
def stemming(text):
    if not isinstance(text, str):
        text = str(text)
    return " ".join(ps.stem(word) for word in text.split())

def tokenize(text):                    
    return text.split()

def preprocessing(text):
    text = toLower(text)
    text = remove_punc(text)
    text = remove_stopword(text)
    text = stemming(text)
    text = tokenize(text)
    return text

def batch_preprocessing(ans):
    return [preprocessing(i) for i in ans]

# Scoring from eval.py
def semantic_emb(model, ans):
    return [model.encode(i, convert_to_tensor=True) for i in ans]

def cos_sim_score(emb_key, emb_ans):
    return [util.cos_sim(i, emb_key).item() for i in emb_ans]

def tfidf_similarity(text_preprocessed, text_list_preprocessed):
    # text_preprocessed is a list of tokens
    text = " ".join(text_preprocessed)
    text_list = [" ".join(t) for t in text_list_preprocessed]

    corpus = [text] + text_list
    vectors = vectorizer.fit_transform(corpus)

    input_vec = vectors[0]
    list_vecs = vectors[1:]
    similarities = cosine_similarity(input_vec, list_vecs).flatten()
    return similarities.tolist()

def give_marks(bert_score, tfidf_score, sentence_level_score):
    marks = []
    for i in range(len(bert_score)):
        # Combined score formula from eval.py
        score = (0.6 * float(bert_score[i]) + 0.15 * float(tfidf_score[i]) + 0.25 * float(sentence_level_score[i])) * 10
        # marks.append(min(math.ceil(score), 10))
        marks.append(min(round(score), 10))
    return marks
    
def sentence_sim_for_each(model, emb_key_list, answer):
    answ = answer.split('.')
    # Remove empty sentences
    answ = [s.strip() for s in answ if s.strip()]
    if not answ: return 0.0
    
    emb_answ = semantic_emb(model, answ)
    s = []
    for i in emb_answ:
        p = []
        for j in emb_key_list:
            p.append(util.cos_sim(i, j).item())
        s.append(max(p) if p else 0.0)
    return sum(s) / len(s) if s else 0.0

def sentence_level_sim(model, key, ans):
    key_s = key.split('.')
    key_s = [s.strip() for s in key_s if s.strip()]
    if not key_s: return [0.0] * len(ans)
    
    emb_key_list = semantic_emb(model, key_s)
    return [sentence_sim_for_each(model, emb_key_list, i) for i in ans]

def process_task(task_path):
    print(f"Processing task: {task_path}")
    try:
        with open(task_path, 'r') as f:
            task = json.load(f)
        
        connections = task.get('connections', [])
        if not connections:
            print("No connections found in task.")
            return

        # Load model (offline)
        print("Loading similarity model...")
        model = SentenceTransformer(MODEL_PATH)
        
        # Group connections by reference file for batch processing, means if there are multiple reference files, group them together
        grouped = {}
        for conn in connections:
            ref = conn['ref_path']
            if ref not in grouped:
                grouped[ref] = []
            grouped[ref].append(conn)

        all_results = []
        for ref_path, conns in grouped.items(): 
            print(f"Processing reference: {ref_path}")
            ref_text = extract_text_from_file(ref_path)
            
            resp_texts = []
            for conn in conns:
                resp_texts.append(extract_text_from_file(conn['resp_path']))

            # 1. Semantic Scores (BERT/SentenceTransformer)
            emb_key = model.encode(ref_text, convert_to_tensor=True)
            emb_ans = semantic_emb(model, resp_texts)
            semantic_scores = cos_sim_score(emb_key, emb_ans)

            # 2. Sentence Level Scores
            sentence_level_scores = sentence_level_sim(model, ref_text, resp_texts)

            # 3. TF-IDF Scores
            key_preprocessed = preprocessing(ref_text)
            ans_preprocessed = batch_preprocessing(resp_texts)
            tfidf_scores = tfidf_similarity(key_preprocessed, ans_preprocessed)

            # 4. Marking
            marks = give_marks(semantic_scores, tfidf_scores, sentence_level_scores)

            for i, conn in enumerate(conns):
                all_results.append({
                    'ref': conn['ref_name'],
                    'resp': conn['resp_name'],
                    'semantic': semantic_scores[i] * 100,
                    'tfidf': tfidf_scores[i] * 100,
                    'sentence': sentence_level_scores[i] * 100,
                    'marks': marks[i]
                })

        # Create Excel File (Simpler)
        wb = Workbook()
        ws = wb.active
        ws.title = "Results"
        ws.append(["Reference File", "Response File", "Marks (0-10)"])
        
        for res in all_results:
            ws.append([res['ref'], res['resp'], res['marks']])
        
        # Use the task filename to create a predictable output filename
        task_id = os.path.splitext(os.path.basename(task_path))[0]
        output_filename = f"Results_{task_id}.xlsx"
        output_path = os.path.join(OUTPUT_DIR, output_filename)
        wb.save(output_path)
        print(f"Results saved to: {output_path}")

        # Move task to completed
        shutil.move(task_path, os.path.join(COMPLETED_DIR, os.path.basename(task_path)))
        
    except Exception as e:
        print(f"Failed to process task {task_path}: {e}")

def main():
    print("DrumsModel listener started. Watching for JSON tasks...")
    while True:
        # Look for JSON files in the root that match the format {username}_{timestamp}.json
        # But we'll just process any .json that isn't in completed_tasks/ or other known files
        for filename in os.listdir(TASK_DIR):
            if filename.endswith(".json") and "_" in filename:
                # Basic check to avoid processing non-task JSONs if they exist
                task_path = os.path.join(TASK_DIR, filename)
                process_task(task_path)
        
        time.sleep(5) # Poll every 5 seconds

if __name__ == "__main__":
    main()
#Workflow of two files for comparison :  FA | FB --uploaded by socket-> JSON -> drumsmodel.py -> main() -> process_task() -> model loading -> Extract text from file ->  Semantic Similarity -> TF-IDF -> Sentence Level -> Final Score -> Excel File 
