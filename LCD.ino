#include <LiquidCrystal.h>

// LCD pins
#define PIN_LCD_RS 12
#define PIN_LCD_EN 11
#define PIN_LCD_D4 5
#define PIN_LCD_D5 4
#define PIN_LCD_D6 3
#define PIN_LCD_D7 2 

// connect either 2 buttons or 1 joystick (1 axis is enough)
// set define to false if unconnected
#define USING_JOYSTICK true
#define USING_BUTTONS true

// two digital button pins
#define PIN_BUTTON_UP 8
#define PIN_BUTTON_DOWN 7
// analog joystick axis pin
#define PIN_JOYSTICK A5
// unconnected floating pin for random seed
#define PIN_RANDOM A0


// Bitmaps

const byte bitmap_dino_up_1[4] = {
  0b00001,
  0b11011,
  0b00110,
  0b01010
};

const byte bitmap_dino_up_2[4] = {
  0b00001,
  0b11011,
  0b00110,
  0b00101
};

const byte bitmap_dino_down_1[4] = {
  0b00000,
  0b00000,
  0b11111,
  0b01010
};

const byte bitmap_dino_down_2[4] = {
  0b00000,
  0b00000,
  0b11111,
  0b00101
};

const byte bitmap_cactus_1[4] = {
  0b01000,
  0b01000,
  0b01100,
  0b01110
};

const byte bitmap_cactus_2[4] = {
  0b00000,
  0b01010,
  0b01110,
  0b00100
};

const byte bitmap_cactus_3[4] = {
  0b00000,
  0b00000,
  0b10010,
  0b11110
};

const byte bitmap_bird[4] = {
  0b00010,
  0b10101,
  0b11110,
  0b00011
};

// const byte bitmap_boss[4] = {
//   0b10001,
//   0b00100,
//   0b10001,
//   0b11111
// };

typedef struct GraphicItem {
  // position
  int position_x;
  int position_y;

  // representation
  const byte *bitmap;

  // todo: add variable size bitmap, currentely is a byte[4]
} GraphicItem;

/*
GraphicsLCD class extends LiquidCrystal to allow individual pixel toggling over a 40 (5*8) by 8 canvas in the upper left corner of the LCD screen.
It uses all the allowed custom characters and changes their bitmaps dynamically for them to match an internal "frame_buffer".
*/
class GraphicsLCD: public LiquidCrystal {
  private:
    byte frame_buffer[8][8];

  public:
    using LiquidCrystal::LiquidCrystal;

    void erase_buffer() {
      memset(frame_buffer, 0, sizeof(frame_buffer));
    }    

    // update each custom character
    void display_buffer() {
      for (int i = 0; i < 8; i++) {
        createChar(i, frame_buffer[i]);    
      }
    }
    /*
    returns true if overwrites an already toggled (on) pixel
    This is used to detect a collision: first the obstacles (cacti and birds) are drawn, then the dino.
    If during the drawing of the dino, one if its pixels overlaps with a pixel of the obstacles, a collision is detected.
    */
    bool set_pixel(int x, int y) {
      if ( (0 <= x && x <= 39) && (0 <= y && y <= 7) ) {
        int sector = floor(x/5);
        int relative_x = 4-(x%5);
        bool overlapping = (frame_buffer[sector][y] >> relative_x) & 1;
        frame_buffer[sector][y] |= 1UL << relative_x;
        return overlapping;
      } else {
        return false;
      }
    }

    bool draw_object(const GraphicItem& item) {
      bool overlapping = false;
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 5; x++) {
          if ((*(item.bitmap + y) >> (4-x)) & 1) {
            overlapping |= set_pixel(item.position_x + x, item.position_y + y);
          }
        }
      }
      return overlapping;
    }
};


#define JOYSTICK_THRESHOLD_UP 650
#define JOYSTICK_THRESHOLD_DOWN 350

bool isUpKeyPressed(void) {  
  bool is_pressed = false;
  if (USING_JOYSTICK) {
    is_pressed |= analogRead(PIN_JOYSTICK) > JOYSTICK_THRESHOLD_UP;
  }
  if (USING_BUTTONS) {
    is_pressed |= digitalRead(PIN_BUTTON_UP);
  }
  return is_pressed;
}

bool isDownKeyPressed(void) {  
  bool is_pressed = false;
  if (USING_JOYSTICK) {
    is_pressed |= analogRead(PIN_JOYSTICK) < JOYSTICK_THRESHOLD_DOWN;
  }
  if (USING_BUTTONS) {
    is_pressed |= digitalRead(PIN_BUTTON_DOWN);
  }
  return is_pressed;
}

// maximum number of obstacles, this value doesn't change the spawn rate so unless it overflows it shouldn't impact anything
#define MAX_ITEMS 10
// delay between successive frames
#define FRAME_DELAY 50

class LCDJumpGame {
  private:
    int iteration;
    int delay_time;
    GraphicsLCD *lcd;

    GraphicItem items[MAX_ITEMS];
    GraphicItem runner;

    int jump_start;
    int last_obstacle_iteration;
    int best = 0;

    bool render_frame() {
      lcd->erase_buffer();

      // draw decor to frame buffer
      for (int i = 0; i < MAX_ITEMS; i++) {
        GraphicItem *item = &items[i];
        if (item->bitmap != 0) {
          lcd->draw_object(*item);
        }
      }

      // draw runner, gameover if overlaps
      bool game_over = lcd->draw_object(runner);

      // update all sectors on screen
      lcd->display_buffer();

      return game_over;
    }

  public:
    LCDJumpGame(GraphicsLCD *graphics_instance) {
      lcd = graphics_instance;
    }    

    void start_game() {
      // remove all objects
      for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].bitmap = (byte*) 0;
      }
      
      lcd->clear();
      lcd->setCursor(0, 0);
      for (int i = 0; i < 8; i++) {
        lcd->write(byte(i));
      }

      iteration = 0;
      // 50
      delay_time = FRAME_DELAY;
      last_obstacle_iteration = 15;

      runner.position_y = 4;
      runner.position_x = 0;
      runner.bitmap = bitmap_dino_up_1;
    }

    void game_over() {
      int score = iteration;

      if (best < score) {
        best = score;
      }
      
      lcd->clear();
      lcd->setCursor(3, 0);
      lcd->write("Game Over!");
      lcd->setCursor(0, 1);
      lcd->write("Best: ");
      lcd->write(String(best).c_str());
    }

    void update_objects_position() {
      // update objects positions
      for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].bitmap == 0) {
          continue;
        }
        
        if (items[i].position_x == -5) {
          items[i].bitmap = 0;
        } else {
          items[i].position_x -= 1;
        }
      }      
    }

    void add_object() {
      const byte * cactuses_bitmap[] = {
        bitmap_cactus_1,
        bitmap_cactus_2,
        bitmap_cactus_3
      };

      bool is_far_away = (iteration - last_obstacle_iteration) >= 17;

      if (is_far_away && random(2))  {
        for (int i = 0; i < MAX_ITEMS; i++) {
          if (items[i].bitmap == 0) {
            Serial.println("obstacle");
            if (random(0, 4) == 0) {
              items[i].bitmap = bitmap_bird;
              items[i].position_x = 40;
              items[i].position_y = 1;
            } else {
              items[i].bitmap = cactuses_bitmap[random(3)];
              items[i].position_x = 40;
              items[i].position_y = 4;
            }
            last_obstacle_iteration = iteration;
            break;
          }
        }
      }
    }

    void handle_running_state(const byte *bitmap_1, const byte *bitmap_2) {
      if (runner.bitmap != bitmap_1 && runner.bitmap != bitmap_2) {
        runner.bitmap = bitmap_1;
      } else if (iteration % 4 == 0) {
        runner.bitmap = (runner.bitmap == bitmap_1) ? bitmap_2 : bitmap_1;
      }
    }

    void handle_jump() {
      // continue jump
      int iterations_since_jump = iteration - jump_start;
      if (iterations_since_jump % 2 == 0) {
        if (iterations_since_jump <= 6) {
          runner.position_y -= 1;
        } else if (iterations_since_jump >= 10) {
          runner.position_y += 1;
        }
      }
    }

    void update_player_position() {
      if (runner.position_y == 4) {
        if (isDownKeyPressed()) {
          // player is crouching
          handle_running_state(bitmap_dino_down_1, bitmap_dino_down_2);
        } else if (isUpKeyPressed()) {
          // start jumping
          runner.bitmap = bitmap_dino_up_1;
          runner.position_y = 3;    
          jump_start = iteration;
        } else {
          handle_running_state(bitmap_dino_up_1, bitmap_dino_up_2);            
        }
      } else {
        handle_jump();
      }
    }

    void display_score() {
      int score = iteration;
      lcd->setCursor(2, 2);
      lcd->write(String(score).c_str());
    }

    bool run() {
      if (render_frame()) {
        delay(2000);
        return false;
      } else {
        iteration += 1;
        delay(delay_time);
      }

      update_player_position();

      update_objects_position();

      add_object();

      display_score();

      return true;
    }
};

GraphicsLCD lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
LCDJumpGame game(&lcd);

void setup() {
  // Serial.begin(9600);

  lcd.begin(16, 2);
  pinMode(PIN_BUTTON_UP, INPUT);
  pinMode(PIN_BUTTON_DOWN, INPUT);

  pinMode(PIN_RANDOM, INPUT);
  randomSeed(analogRead(PIN_RANDOM));
}

void loop() {
  game.start_game();
  while (game.run());
  game.game_over();
  delay(2000);
}
