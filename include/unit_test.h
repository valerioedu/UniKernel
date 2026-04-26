#ifndef UNIT_TEST_H
#define UNIT_TEST_H

typedef enum unitTestScenario {
    HEAP_STRESS = 0,
    HEAP_FRAGMENT,
} unitTestScenario;

[[__noreturn__]]void unitTestBegin(unitTestScenario scenario);

#endif