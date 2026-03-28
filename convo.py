import sys
import os
import fitz  # PyMuPDF
from paddleocr import PaddleOCR
import numpy as np
import io
from PIL import Image

# Disable the slow model source check and telemetry at the global level
os.environ["PADDLE_PDX_DISABLE_MODEL_SOURCE_CHECK"] = "True"

def pdf_to_images(pdf_path):
    """Converts PDF pages to a list of PIL Images."""
    print(f"Opening PDF: {pdf_path}")
    doc = fitz.open(pdf_path)
    images = []
    for i, page in enumerate(doc):
        # 2.0x zoom handles handwriting much better than default resolution
        pix = page.get_pixmap(matrix=fitz.Matrix(2, 2))
        img_data = pix.tobytes("png")
        images.append(Image.open(io.BytesIO(img_data)))
        print(f"  Captured page {i+1}")
    doc.close()
    return images

def perform_ocr(images):
    """Performs OCR on a list of images using PaddleOCR."""
    print("Initializing PaddleOCR (this may download model weights if not found)...")
    
    # lang='en' for English. 
    # use_textline_orientation replaces deprecated use_angle_cls
    ocr = PaddleOCR(use_textline_orientation=True, lang='en')
    
    full_text = []
    for i, img in enumerate(images):
        print(f"Processing page {i+1}/{len(images)}...")
        # Convert PIL Image to numpy array for PaddleOCR compatibility
        img_np = np.array(img)
        # PaddleOCR returns a list of results (one list per image)
        result = ocr.ocr(img_np, cls=True)
        
        page_lines = []
        if result and result[0]:
            for line in result[0]:
                # line[1][0] is the text string, line[1][1] is confidence
                text = line[1][0]
                page_lines.append(text)
        
        full_text.append("\n".join(page_lines))
        
    return "\n\n--- Page Break ---\n\n".join(full_text)

def main():
    if len(sys.argv) < 2:
        print("Usage: python convo.py <pdf_path_or_--test>")
        sys.exit(1)
    
    path = sys.argv[1]
    
    if path == "--test":
        print("Running preliminary check...")
        try:
            from paddleocr import PaddleOCR
            import fitz
            print("Successfully imported PaddleOCR and PyMuPDF.")
            sys.exit(0)
        except ImportError as e:
            print(f"Import Error: {e}")
            print("Please ensure you have installed the requirements in your .venv.")
            sys.exit(1)

    if not os.path.exists(path):
        print(f"Error: File '{path}' not found.")
        sys.exit(1)
        
    if not path.lower().endswith(".pdf"):
        print(f"Error: '{path}' is not a PDF file.")
        sys.exit(1)

    try:
        images = pdf_to_images(path)
        print(f"Starting OCR on {len(images)} pages...")
        text = perform_ocr(images)
        
        print("\n" + "="*50)
        print("EXTRACTED TEXT OUTPUT")
        print("="*50 + "\n")
        print(text)
        print("\n" + "="*50 + "\n")
        
    except Exception as e:
        print(f"An error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
