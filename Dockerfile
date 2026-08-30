FROM gcc:latest

WORKDIR /app

# Copy all project files into the container
COPY . .

# Initialize users & questions database, then compile the server
RUN gcc init_data.c -o init_data && ./init_data
RUN gcc backend.c -o backend

# Expose port 8080 for web traffic
EXPOSE 8080

# Run the server 24/7
CMD ["./backend"]
