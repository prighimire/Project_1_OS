#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    sleep(5); // Simulate background task taking 5 seconds

    // Open the file for writing
    int fd = open("background.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error creating file");
        return 1;
    }

    // Write "Background process completed" to the file
    dprintf(fd, "Background process completed\n");

    // Close the file
    close(fd);

    return 0;
}

