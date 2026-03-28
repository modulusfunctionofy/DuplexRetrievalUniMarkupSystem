import easyocr
import cv2

def extract_words_easy(image_path):
    """Extract words from image using EasyOCR"""
    # 'en' for English, add more like ['en', 'hi'] for Hindi
    reader = easyocr.Reader(['en'], gpu=True)  # Set gpu=True if you have CUDA
    print("Processing image...")
    results = reader.readtext(image_path)
    
    print("\n" + "="*50)
    print("EXTRACTED TEXT")
    print("="*50)
    
    if results:
        for i, (bbox, text, confidence) in enumerate(results, 1):
            print(f"{i:2d}. {text} (confidence: {confidence:.2f})")
        
        print(f"\nTotal words found: {len(results)}")
        
        # Show all text combined
        all_text = " ".join([text for _, text, _ in results])
        print(f"\nAll text combined :---------------\n{all_text}")
    else:
        print("No text found in the image!")

if __name__ == "__main__":
    image_path = "folderUSER/lfimgsUSER/WhatsApp Image 2026-03-23 at 21.42.54.jpeg"
    
    extract_words_easy(image_path)
