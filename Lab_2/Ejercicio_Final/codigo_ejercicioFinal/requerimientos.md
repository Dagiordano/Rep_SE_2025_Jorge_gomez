#Esp32Cam debe sacar fotos y guardarlas continuamente el el espacio de memoria que prefiramos. Se deben almacenar 20 fotos, si hay más . se borra la primera. a cada una de estas fotos hay que aplicarle un histograma y un filtro Sobel.


1. Muestre la salida obtenida al graficar en Colab 5 im´agenes consecutivas. Entregable:
Sucesi´on de 5 im´agenes (0.2 puntos).



O sea debemos poder sacar fotos, guardarlas y poder recuperarlas en el computador, guardar los filtros y el histograma. (si no hay espacio borrar la foto og y dejar histogramas y filtro)





2. Determine cu´antos frames puede procesar y guardar por segundo (FPS). Entregable:
FPS (0.2 puntos).

3. Determine si el cuello de botella est´a en la captura de imagen, ecualizaci´on de histograma, filtro Sobel o guardado de la im´agen. Entregable: Gr´afico de barras con el
tiempo de cada operaci´on (0.2 puntos).

4. Mida la potencia de su aplicaci´on y determine cu´anta energ´ıa consume al procesar y
guardar un frame. Entregable: Medici´on y c´alculo (0.2 puntos).

5. Cambie la frecuencia del microcontrolador y, para cada opci´on de frecuencia, determine
FPS y energ´ıa por frame. Entregable: Gr´aficos de FPS vs frecuencia y energ´ıa vs
frecuencia (0.3 puntos).

6. Estime la cantidad m´ınima de energ´ıa que debe tener una bater´ıa para soportar su
aplicaci´on funcionando durante 10 d´ıas a su m´aximo FPS. Entregable: C´alculos (0.2
puntos).

7. Competencia: Se define la medida de desempe˜no como FPS/Potencia(W). Determine
en qu´e frecuencia se maximiza esta medida. Entregable: FPS/W m´aximo para su
configuraci´on (Distribuci