from PIL import Image
import numpy as np
import requests
from io import BytesIO
import matplotlib.pyplot as plt

def process_image(image_url, output_size=(96, 96), output_file='imagen_96x96.raw', show_images=True):
    # Download the image
    response = requests.get(image_url)
    if response.status_code != 200:
        raise Exception(f"Failed to download image. Status code: {response.status_code}")
    
    # Open the image from bytes and process it
    original_img = Image.open(BytesIO(response.content))
    
    # Convert to grayscale and resize
    gray_img = original_img.convert('L').resize(output_size)
    
    # Convert to array and save as binary
    arr = np.array(gray_img, dtype=np.uint8)
    arr.tofile(output_file)
    
    print(f"Image processed and saved as {output_file}")
    
    if show_images:
        # Create a figure with two subplots side by side
        plt.figure(figsize=(12, 6))
        
        # Display original image
        plt.subplot(1, 2, 1)
        plt.imshow(original_img)
        plt.title('Original Image')
        plt.axis('off')
        
        # Display grayscale image
        plt.subplot(1, 2, 2)
        plt.imshow(gray_img, cmap='gray')
        plt.title('Grayscale Image')
        plt.axis('off')
        
        plt.tight_layout()
        plt.show()
    
    return gray_img

# Example usage
if __name__ == "__main__":
    # Example image URL (you can replace this with any image URL)
    image_url = "https://estaticos-cdn.prensaiberica.es/clip/a841a559-bfe3-429e-acfa-d29872a4a852_16-9-discover-aspect-ratio_default_0.jpg"
    try:
        processed_image = process_image(image_url)
        print("Image processing completed successfully!")
    except Exception as e:
        print(f"Error processing image: {e}")
