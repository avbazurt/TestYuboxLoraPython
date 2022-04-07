# TestYuboxLoraPython
El codigo en este repositorio es para envio y recepcion de datos mediante modulo Lora. Para cada ejemplo se presenta dos codigos, los terminados en _rpi son para ejecutar dentro de la Raspberry con el modulo Lora, por otro lado los terminados en _pc son para ser ejecutados en una PC, Lapto u otro dispositivo donde se pueda ejecutar python.

## Contenido
- Ejemplo #1: Envio datos mediante Lora desde RaspberryPi.

- Ejemplo #2 Recibir datos en RaspberryPi mediante Lora.

## Requisitos
Para los codigos _pc, se necesita conectar al MQTT de la RedLora, para ello utilizamos la libreria [paho-mqtt](https://github.com/eclipse/paho.mqtt.python), dentro del repositorio de la libreria estan las instrucciones de como instalarlo en sus dispositivo. Si el estudiante desconoce de estos temas, preparamos el archivo requirements.txt para faciltar el proceso. Abra una terminal de comandos y ejecute el siguiente comando:


      pip3 install -r requirements.txt


