// Em musics.h (ou no header apropriado)
typedef struct {
    uint frequency;
    uint duration; 
} Note;

// Array de notas modificado para usar a struct
const Note intro_melody[] = {
    {.frequency = 392*8, .duration = 550},
    {.frequency = 311*8, .duration = 500},
    {.frequency = 466*8, .duration = 250},
    {.frequency = 392*8, .duration = 750}
};

const Note alarm_melody[] = {
    {.frequency = 311*8, .duration = 500},
    {.frequency = 466*8, .duration = 500},
    {.frequency = 311*8, .duration = 500},
    {.frequency = 466*8, .duration = 500}
};