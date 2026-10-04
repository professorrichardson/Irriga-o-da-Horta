# Irrigação Automática da Horta com Reaproveitamento de Água

Projeto apresentado no **Paraná Faz Ciência**.

Sistema com Arduino UNO que irriga uma horta com sprinklers e devolve a água que sobra para o reservatório, evitando desperdício.

## Como funciona

O sistema tem três níveis:

1. **Reservatório de alta:** guarda a água. Uma bomba de alta pressão (12V) manda a água para os sprinklers.
2. **Horta:** recebe a água dos sprinklers.
3. **Reservatório do meio:** recebe a água que sobra da horta. Nele ficam um sensor de nível e uma bomba submersa (5V), que devolve a água para o reservatório de alta.

Ciclo de funcionamento:

1. Ao apertar o botão, a bomba de alta liga e a horta é irrigada.
2. Quando o sensor detecta água no reservatório do meio, a bomba de alta desliga.
3. Depois de 5 segundos, a bomba submersa liga e devolve a água para o reservatório de alta.
4. Quando o sensor não detecta mais água, a bomba submersa desliga e o ciclo termina.

Por segurança, apertar o botão durante o ciclo desliga tudo. Cada bomba também desliga sozinha se ficar ligada mais de 10 minutos.

## Materiais

- Arduino UNO
- Módulo relé de 2 canais (bobina 5V)
- Sensor de chuva com módulo LM393
- Botão (push button)
- Bomba de alta pressão 12V e fonte 12V
- Bomba submersa 5V e fonte 5V
- Fios jumper e protoboard

## Ligações

| Arduino | Ligado em |
|---------|-----------|
| 5V | VCC do sensor e VCC do módulo relé |
| GND | Botão, GND do sensor e GND do módulo relé |
| D2 | Botão |
| D3 | Saída DO do sensor |
| D7 | IN1 do relé (bomba de alta) |
| D8 | IN2 do relé (bomba submersa) |

O diagrama completo está no tutorial: [IrrigacaoHorta/tutorial.html](IrrigacaoHorta/tutorial.html).

## Como usar

1. Instale a [Arduino IDE](https://www.arduino.cc/en/software). Não é preciso instalar nenhuma biblioteca.
2. Abra o arquivo [IrrigacaoHorta/IrrigacaoHorta.ino](IrrigacaoHorta/IrrigacaoHorta.ino).
3. Em **Ferramentas**, escolha a placa **Arduino Uno** e a porta do Arduino.
4. Clique em **Carregar**.
5. Abra o **Monitor Serial** em 9600 baud para acompanhar cada etapa do ciclo.

## Estrutura

```
IrrigacaoHorta/
├── IrrigacaoHorta.ino   # código do Arduino, todo comentado
└── tutorial.html        # tutorial com diagrama de ligação e testes
```

## Autores

- Nome do aluno
- Nome do orientador
- Escola / Cidade - PR
