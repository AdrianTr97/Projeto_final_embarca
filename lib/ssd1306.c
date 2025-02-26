#include "ssd1306.h"
#include "font.h"

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
  ssd->width = width;
  ssd->height = height;
  ssd->pages = height / 8U;
  ssd->address = address;
  ssd->i2c_port = i2c;
  ssd->bufsize = ssd->pages * ssd->width + 1;
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
  ssd->ram_buffer[0] = 0x40;
  ssd->port_buffer[0] = 0x80;
}

void ssd1306_config(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_DISP | 0x00);
  ssd1306_command(ssd, SET_MEM_ADDR);
  ssd1306_command(ssd, 0x01);
  ssd1306_command(ssd, SET_DISP_START_LINE | 0x00);
  ssd1306_command(ssd, SET_SEG_REMAP | 0x01);
  ssd1306_command(ssd, SET_MUX_RATIO);
  ssd1306_command(ssd, HEIGHT - 1);
  ssd1306_command(ssd, SET_COM_OUT_DIR | 0x08);
  ssd1306_command(ssd, SET_DISP_OFFSET);
  ssd1306_command(ssd, 0x00);
  ssd1306_command(ssd, SET_COM_PIN_CFG);
  ssd1306_command(ssd, 0x12);
  ssd1306_command(ssd, SET_DISP_CLK_DIV);
  ssd1306_command(ssd, 0x80);
  ssd1306_command(ssd, SET_PRECHARGE);
  ssd1306_command(ssd, 0xF1);
  ssd1306_command(ssd, SET_VCOM_DESEL);
  ssd1306_command(ssd, 0x30);
  ssd1306_command(ssd, SET_CONTRAST);
  ssd1306_command(ssd, 0xFF);
  ssd1306_command(ssd, SET_ENTIRE_ON);
  ssd1306_command(ssd, SET_NORM_INV);
  ssd1306_command(ssd, SET_CHARGE_PUMP);
  ssd1306_command(ssd, 0x14);
  ssd1306_command(ssd, SET_DISP | 0x01);
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command) {
  ssd->port_buffer[1] = command;
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->port_buffer,
    2,
    false
  );
}

void ssd1306_send_data(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_COL_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->width - 1);
  ssd1306_command(ssd, SET_PAGE_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->pages - 1);
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->ram_buffer,
    ssd->bufsize,
    false
  );
}
//O display SSD1306 geralmente é organizado em uma matriz de pixels, onde cada byte representa 8 pixels verticais (em um esquema de "coluna x linha")
//A resolução do display é 128x64 pixels, mas sua memória interna é organizada em 128 colunas e 8 páginas verticais. sendo na pratica 128x8
//Ou seja, a memória é mapeada de maneira que cada página contém 8 pixels de altura e a tela tem 128 colunas.
//128x8 para o armazenamento interno da memória do SSD1306, mas visualmente o display possui 128x64 pixels em sua resolução.
//uint8_t x: Coordenada X do pixel (coluna), uint8_t y: Coordenada Y do pixel (linha).
//bool value: Valor para definir o pixel. true acende o pixel e false apaga o pixel.
void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value) {
  uint16_t index = (y >> 3) + (x << 3) + 1; //como dito na organização de memória cada byte armazena 8 pixels verticais (de 8 linhas), uma pagina
  // A linha y é dividida por 8 (feito por y >> 3, deslocamento de 3 bits), para determinar a página do display correspondente
  //x << 3 desloca a coordenada x para encontrar a coluna correta. creio que o +1 é para compensar o início do mapeamento de memória, dependendo da implementação.
  uint8_t pixel = (y & 0b111); //0b111 é uma máscara binária que seleciona os 3 bits menos significativos do número. ja que 0b111 (00000111, em 8 bits) AND (&) a posicao 
  //de y que resulta no número do pixel dentro de uma "página" de 8 pixels
  //Quando y = 10, A coordenada de y é 10, em binario, y = 00001010, y & 0b111 irá resultar em 2, ou seja, o pixel esta na posicao 2 de sua pagina
  if (value)
    ssd->ram_buffer[index] |= (1 << pixel); //->: Usado para acessar membros de uma estrutura através de um ponteiro, em vez de usar o operador . com uma variável de estrutura
  else
    ssd->ram_buffer[index] &= ~(1 << pixel);
}

/*
void ssd1306_fill(ssd1306_t *ssd, bool value) {
  uint8_t byte = value ? 0xFF : 0x00;
  for (uint8_t i = 1; i < ssd->bufsize; ++i)
    ssd->ram_buffer[i] = byte;
}*/

void ssd1306_fill(ssd1306_t *ssd, bool value) {
    // Itera por todas as posições do display
    for (uint8_t y = 0; y < ssd->height; ++y) {
        for (uint8_t x = 0; x < ssd->width; ++x) {
            ssd1306_pixel(ssd, x, y, value);
        }
    }
}
//ssd1306_t *ssd: Um ponteiro para a estrutura do display SSD1306, que contém o buffer de pixels.
//uint8_t top: A coordenada Y do topo do retângulo (onde o retângulo começa verticalmente).
//uint8_t left: A coordenada X da esquerda do retângulo (onde o retângulo começa horizontalmente).
//uint8_t width: A largura do retângulo (quantos pixels ele ocupará horizontalmente).
//uint8_t height: A altura do retângulo (quantos pixels ele ocupará verticalmente).
//bool value: Um valor booleano que determina se o retângulo será desenhado com pixels acesos (true) ou apagados (false).
//bool fill: Um valor booleano que determina se o retângulo será preenchido ou apenas contornado.
void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill) {
  for (uint8_t x = left; x < left + width; ++x) {
    ssd1306_pixel(ssd, x, top, value);
    ssd1306_pixel(ssd, x, top + height - 1, value);
  }
  for (uint8_t y = top; y < top + height; ++y) {
    ssd1306_pixel(ssd, left, y, value);
    ssd1306_pixel(ssd, left + width - 1, y, value);
  }

  if (fill) {
    for (uint8_t x = left + 1; x < left + width - 1; ++x) {
      for (uint8_t y = top + 1; y < top + height - 1; ++y) {
        ssd1306_pixel(ssd, x, y, value);
      }
    }
  }
}

void ssd1306_line(ssd1306_t *ssd, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (true) {
        ssd1306_pixel(ssd, x0, y0, value); // Desenha o pixel atual

        if (x0 == x1 && y0 == y1) break; // Termina quando alcança o ponto final

        int e2 = err * 2;

        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}


void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value) {
  for (uint8_t x = x0; x <= x1; ++x)
    ssd1306_pixel(ssd, x, y, value);
}

void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value) {
  for (uint8_t y = y0; y <= y1; ++y)
    ssd1306_pixel(ssd, x, y, value);
}

// Função para desenhar um caractere
void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y)
{
  uint16_t index = 0;
  char ver=c; //variavel extra para depuracao
  if (c >= 'A' && c <= 'Z') //lógica para mapear os caracteres para a tabela de fontes, verifica se o caractere c está entre 'A' e 'Z', letras maiúsculas
  {
    index = (c - 'A' + 15) * 8; //calcula o índice index para encontrar os dados da fonte no array que contém os pixels dos caracteres.  
    //O valor (c - 'A') converte a letra maiúscula em um número de 0 a 25 (por exemplo, 'A' vira 0, 'B' vira 1, ..., 'Z' vira 25).
    // se a letra for C, 'C' - 'A' = 2, entao, index = (2 + 11) * 8 = 13 * 8 = 104
    //os 14 caracteres anterios sao os numeros, simbolos especiais e o nothing, entao comeca no 13, *8 porque cada caractere ocupa 8 bytes em font.h
  }else  if (c >= 'a' && c <= 'z')
  {
    index = (c - 'a' + 41) * 8; //
  }else  if (c >= '0' && c <= '9')
  {
    index = (c - '0' + 1) * 8; // Adiciona o deslocamento necessário
  }else if (c == ':') 
  {
    index = 11 * 8;  // Índice do dois pontos
  }
  else if (c == '!') 
  {
    index = 12 * 8;  // indice de !
  }else if (c == '@') // '@' represente o simbolo do fan(ventilador) 
  {
    index = 13 * 8;  
  }
  else if (c == ';') // ';' represente o simbolo de perigo(warning)
  {
    index = 14 * 8;  
  }
  
  for (uint8_t i = 0; i < 8; ++i)
  {
    uint8_t line = font[index + i];
    for (uint8_t j = 0; j < 8; ++j)
    {
      ssd1306_pixel(ssd, x + i, y + j, line & (1 << j));
    }
  }
}

// Função para desenhar uma string
void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y)
{ //x: A coordenada horizontal (posição da coluna) onde o texto começará, x pode variar de 0 a 127, representando a posição horizontal no display. 
  //y: A coordenada vertical (posição da linha) onde o texto será desenhado.
  while (*str)
  {
    ssd1306_draw_char(ssd, *str++, x, y);
    x += 8;
    if (x + 8 >= ssd->width)
    {
      x = 0;
      y += 8;
    }
    if (y + 8 >= ssd->height)
    {
      break;
    }
  }
}
