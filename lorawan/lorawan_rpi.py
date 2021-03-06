from time import sleep
from subprocess import Popen, PIPE, STDOUT, run, TimeoutExpired
from threading import Semaphore,Thread
import shlex
import io
import codecs


class YuboxLora:
	def __init__(self, deveui:str, appkey:str, callbackMessage = None) -> None:
		#Semaphore para controlar flujo
		self.control_threads = Semaphore(1)

		#Comando para iniciar el proceso
		comand = "sudo ./lorawan/lorawan-rpi --deveui {} --appkey {} --subband 2 --skew 10".format(deveui,appkey)

		#Ejecuto el comando con Popen
		self.proceso = Popen(shlex.split(comand),stdin=PIPE,stdout=PIPE)

		#Creo el objeto stdin para pasarle los parametros al proceso principal
		self.stdin = io.TextIOWrapper(
			self.proceso.stdin,
			encoding='utf-8',
			line_buffering=True,  # send data on newline
		)

		#Creo el objeto stdout para recibir los parametros que nos envie el proceso
		self.stdout = io.TextIOWrapper(
			self.proceso.stdout,
			encoding='utf-8',
		)
		
		#Iniciamos con un bucle para detectar cuando EVENT JOIN este en OK
		try:
			bandera = True
			print("EVENT JOIN START")
			while bandera:
				output = self.stdout.readline()
				output = output.rstrip()

				if (output == "EVENT JOIN OK"):
					#Se valida que inicio el proceso y se sale del bucle
					bandera = False
					print("EVENT JOIN OK")

				elif (output == "EVENT JOIN FAIL"):
					print("EVENT JOIN FAIL, REATRY")
		
		except KeyboardInterrupt:
			self.Close()
			print("Se paro el proceso Lora antes de completarse")
			print("Para evitar errores en el codigo, se detiene la ejecucion")
			quit()



		#Variable donde se almacena el mensaje que llega
		self.message_cmd = ""

		#Variable donde se almacena el callback que llega
		#OnMessageLora(ssr: int ,msg: str):
		self.OnMessageLora = callbackMessage

		#Inicio el proceso que se encarga verificar
		#Los mensajes de consola
		Thread(target=self._ReviewStdoutMessage, daemon=True).start()


	def _ReviewStdoutMessage(self):
		while (True):
			#Leemos la salida de consola
			output = ""
			output = self.stdout.readline()
			output = output.rstrip()

			#Procedemos a guardar la salida en la variable
			self.message_cmd = output

			#Verificamos si existe un callback
			if (self.OnMessageLora != None):
				#Procedemos a validar si llego un mensaje para  pasar al callback
				if ("EVENT RX RSSI" in output and "HEXDATA" in output):
					#Output tiene el siguiente formato
					#EVENT RX RSSI -21 SNR 14 HEXDATA 486F6C61436F6D6F
					list_output = output.split(" ")

					#Separamos los valores importante
					rssi = list_output[3]
					hexdata = list_output[7]

					#El mensaje se encuentra en hexadecimal, lo pasamos a string
					binary_str = codecs.decode(hexdata, "hex")
					msg_str = str(binary_str,'utf-8')

					#Pasamos el texto al callback
					self.OnMessageLora(rssi,msg_str)


	def SendData(self, msg:str) -> None:
		with self.control_threads:
			#El proceso requiere pasar un dato en Hexadecimal
			msg_hex = (msg.encode('utf-8')).hex()

			#Proceso a enviar el dato
			self.stdin.write("SEND {}\n".format(msg_hex))

			#Iniciamos un bucle para estar pendientes del mensaje
			bandera = True
			while bandera:
				if ("EVENT PACKET" in self.message_cmd) and ("SENT" in self.message_cmd):
					bandera = False


	def Close(self)-> None:
		#Con esta funcion cerramos el proceso
		with self.control_threads:
			self.stdin.write("EXIT")
			sleep(2)


