import paho.mqtt.client as mqtt
import json
import base64

def on_connect(client, userdata, flags, rc) -> None:
    print("Conectado")
    client.subscribe(f"application/5/device/{userdata['deveui']}/event/up")


def on_disconnet(client, userdata, rc) -> None:
    print("Desconectado")
    client.reconnect_delay_set(min_delay=1, max_delay=120)


def on_message(client, userdata, message) -> None:
    mensaje = json.loads(str(message.payload.decode("utf-8")).strip())
    topico = str(message.topic).strip()

    #Separamos los datos del mensaje
    dataBase64 = mensaje["data"]
    rxInfo = mensaje["rxInfo"][0]
    name_gatewey = rxInfo["name"]

    # El mensaje se encuentra en Base64, lo pasamos a string
    base64_bytes = dataBase64.encode('ascii')
    message_bytes = base64.b64decode(base64_bytes)
    message = message_bytes.decode('ascii')


    #Mostramos por pantalla los resultados
    print("------------------------------------------------------------------------")
    print("Data recibida sin procesar:")
    print(mensaje,"\n")
    print("Data recibida procesada:")
    print(f"Mensaje recibido: {message}, fue transmitido por: {name_gatewey}")
    print("")



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
client = mqtt.Client(userdata={"deveui":deveui})
client.username_pw_set(username=user, password=password)

#Asignamos las funciones callback
client.on_connect = on_connect
client.on_disconnect = on_disconnet
client.on_message = on_message

#Nos conectamos
try:
    client.connect(host, port, 60)
except OSError:
    client.reconnect_delay_set(min_delay=1, max_delay=120)

#Empezamos el bucle
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("Proceso Finalizado")