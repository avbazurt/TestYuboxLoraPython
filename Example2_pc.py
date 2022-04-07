import paho.mqtt.client as mqtt
import json
import base64
from random import  choice
from time import sleep

def on_connect(client, userdata, flags, rc) -> None:
    print("Conectado")

def on_disconnet(client, userdata, rc) -> None:
    print("Desconectado")
    client.reconnect_delay_set(min_delay=1, max_delay=120)

def FraseRandom():
    nombres = ["Vidal", "Juan", "Pepe", "Saul", "Maria", "Elena", "Messi"]
    verbo = ["Comer", "Jugar", "Pelear", "Estudiar"]
    objeto = ["Balon", "Foco", "Matematicas", "Iotodos"]

    #Extraigo elementos random de las listas
    str1 = choice(nombres)
    str2 = choice(verbo)
    str3 = choice(objeto)

    #Creo la frase
    frase = f"{str1} {str2} Con {str3}"

    return frase


#----------------------------------------------
#Importante Cambiar
deveui = "deveui"
#----------------------------------------------

#Credenciales Servidor MQTT para Iotodos Red Lora
user = ""
password = ""
host = "zensor.ec"
port = 1883


#Creeamos los client
client = mqtt.Client()
client.username_pw_set(username=user, password=password)

#Asignamos las funciones callback
client.on_connect = on_connect
client.on_disconnect = on_disconnet


#Nos conectamos
try:
    client.connect(host, port, 60)
except OSError:
    client.reconnect_delay_set(min_delay=1, max_delay=120)

topic = f"application/5/device/{deveui}/command/down"

#Empezamos el bucle
while (True):
    try:
        #Creamos la informacion a enviar
        msg = FraseRandom()

        #Debemos pasarle el mensaje en Base64
        message_bytes = msg.encode('ascii')
        base64_bytes = base64.b64encode(message_bytes)
        base64_message = base64_bytes.decode('ascii')

        #El comando necesita un json
        json_data = {"confirmed":False, "fPort" : 2, "data" : base64_message}

        #Lo convertimos en texto
        msg_json =  json.dumps(json_data)

        #Y lo enviamos a la Raspberry
        print(f"Mensaje publicado: {msg}\n")
        client.publish(topic,msg_json)

        #Esperamos unos segundos antes de enviar otro mensaje
        #Para darle tiempo a la Raspberry de recibir y procesarlo
        sleep(15)

    except KeyboardInterrupt:
        print("Proceso Finalizado")
        break

