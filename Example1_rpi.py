from lorawan.lorawan_rpi import YuboxLora
from random import randint
from time import sleep

#Creamos el objeto YuboxLora y le pasamos nuestras credenciales
YuboxLora = YuboxLora(deveui = "deveui",
						appkey = "appkey")

#Iniciamos el bucle
while True:
	try:
		#Generamos el texto
		texto = "{}-{}".format(randint(1,2500)/100,randint(1,2500)/100)

		#Pasamos el texto
		YuboxLora.SendData(texto)

		#Esperamos un poco para no saturar
		print("Datos Enviados Exitosamente")
		sleep(5)

	except KeyboardInterrupt:
		#Para que no se nos bloque los pines GPIO, debemos cerrar
		YuboxLora.Close()
		break
