from lorawan.lorawan_rpi import YuboxLora
from random import randint
from time import sleep

#Funciones Callback
def OnMessageLora(ssr: int ,msg: str):
	print(f"Mensaje Recibido: {msg}, con un SSR: {ssr}\n")


#Creamos el objeto YuboxLora y le pasamos nuestras credenciales
YuboxLora = YuboxLora(deveui = "deveui",
						appkey = "appkey",
						callbackMessage= OnMessageLora)

#Iniciamos el bucle
while True:
	try:
		sleep(1)

	except KeyboardInterrupt:
		YuboxLora.Close()
		break
